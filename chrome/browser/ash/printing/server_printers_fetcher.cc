// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/server_printers_fetcher.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_event_log/device_event_log.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "third_party/libipp/libipp/builder.h"
#include "third_party/libipp/libipp/frame.h"
#include "third_party/libipp/libipp/parser.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr net::NetworkTrafficAnnotationTag kServerPrintersFetcherNetworkTag =
    net::DefineNetworkTrafficAnnotation("printing_server_printers_query", R"(
    semantics {
      sender: "ChromeOS Printers Manager"
      description:
        "Fetches the list of available printers from the Print Server."
      trigger: "1. User asked for the list of available printers from "
               "a chosen Print Server."
               "2. ChromeOS automatically queries printers from Print "
               "Servers whose addresses are set by the organization's "
               "administrator at the Google admin console."
      data: "None."
      destination: OTHER
      destination_other: "Print Server"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature is enabled as long as printing is enabled."
      chrome_policy {
        PrintingEnabled {
            PrintingEnabled: false
        }
      }
    })");

std::string ServerPrinterId(const std::string& url) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, url);
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return "server-" + base::MD5DigestToBase16(digest);
}

}  // namespace

class ServerPrintersFetcher::PrivateImplementation
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  PrivateImplementation(const ServerPrintersFetcher* owner,
                        Profile* profile,
                        const GURL& server_url,
                        const std::string& server_name,
                        ServerPrintersFetcher::OnPrintersFetchedCallback cb)
      : owner_(owner),
        server_url_(server_url),
        server_name_(server_name),
        callback_(std::move(cb)),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    CHECK(base::SequencedTaskRunner::HasCurrentDefault());
    task_runner_for_callback_ = base::SequencedTaskRunner::GetCurrentDefault();
    // Post task to execute.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PrivateImplementation::SendQuery,
                                  base::Unretained(this),
                                  profile->GetURLLoaderFactory()->Clone()));
  }

  PrivateImplementation(const PrivateImplementation&) = delete;
  PrivateImplementation& operator=(const PrivateImplementation&) = delete;

  ~PrivateImplementation() override = default;

  // Schedule the given object for deletion. May be called from any
  // sequence/thread.
  void Delete() { task_runner_->DeleteSoon(FROM_HERE, this); }

  // Implementation of network::SimpleURLLoaderStreamConsumer.
  void OnDataReceived(std::string_view part_of_payload,
                      base::OnceClosure resume) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    response_.insert(response_.end(), part_of_payload.begin(),
                     part_of_payload.end());
    std::move(resume).Run();
  }

  // Implementation of network::SimpleURLLoaderStreamConsumer.
  void OnComplete(bool success) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!success) {
      const int net_error = simple_url_loader_->NetError();
      PRINTER_LOG(ERROR) << "Error when querying the print server "
                         << server_name_ << ": NetError=" << net_error;
      // Set last_error_, call the callback with an empty vector and exit.
      if (net_error >= -399 && net_error <= -303) {
        last_error_ = PrintServerQueryResult::kHttpError;
      } else if (net_error >= -302 && net_error <= -300) {
        last_error_ = PrintServerQueryResult::kIncorrectUrl;
      } else {
        last_error_ = PrintServerQueryResult::kConnectionError;
      }
      PostResponse({});
      return;
    }
    // Try to parse the response.
    ipp::SimpleParserLog log;
    ipp::Frame response = ipp::Parse(response_.data(), response_.size(), log);
    if (!log.Errors().empty()) {
      // Errors were detected during parsing.
      std::string message =
          "Errors detected when parsing a response from the "
          "print server " +
          server_name_ + ". Parser log:";
      for (const auto& entry : log.Errors()) {
        message += "\n * " + ipp::ToString(entry);
      }
      LOG(WARNING) << message;
    }
    if (!log.CriticalErrors().empty()) {
      // Parser has failed. Dump errors to the log.
      std::string message = "Cannot parse response from the print server " +
                            server_name_ + ". Critical errors:";
      for (const auto& entry : log.CriticalErrors()) {
        message += "\n * " + ipp::ToString(entry);
      }
      LOG(WARNING) << message;
      PRINTER_LOG(ERROR) << "Error when querying the print server "
                         << server_name_ << ": unparsable IPP response.";
      // Set last_error_, call the callback with an empty vector and exit.
      last_error_ = PrintServerQueryResult::kCannotParseIppResponse;
      PostResponse({});
      return;
    }
    // The response parsed successfully. Retrieve the list of printers.
    ipp::CollsView printer_attrs =
        response.Groups(ipp::GroupTag::printer_attributes);
    std::vector<PrinterDetector::DetectedPrinter> printers(
        printer_attrs.size());
    for (size_t i = 0; i < printers.size(); ++i) {
      ipp::Collection::iterator it = printer_attrs[i].GetAttr("printer-name");
      ipp::StringWithLanguage name;
      if (it == printer_attrs[i].end() ||
          it->GetValue(0, name) != ipp::Code::kOK) {
        name.value = "Unknown Printer " + base::NumberToString(i);
      }
      InitializePrinter(&(printers[i].printer), name.value);
    }
    // Call the callback with queried printers.
    PRINTER_LOG(DEBUG) << "The print server " << server_name_ << " returned "
                       << printers.size() << " printer"
                       << (printers.size() == 1 ? "." : "s.");
    PostResponse(std::move(printers));
  }

  // Implementation of network::SimpleURLLoaderStreamConsumer.
  void OnRetry(base::OnceClosure start_retry) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    response_.clear();
    std::move(start_retry).Run();
  }

  PrintServerQueryResult last_error() const { return last_error_; }

 private:
  // The main task. It is scheduled in the constructor.
  void SendQuery(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                     url_loader_factory) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Preparation of the IPP frame.
    ipp::Frame request(ipp::Operation::CUPS_Get_Printers);
    DCHECK_EQ(ipp::Code::kOK,
              request.Groups(ipp::GroupTag::operation_attributes)[0].AddAttr(
                  "requested-attributes", ipp::ValueTag::keyword,
                  "printer-description"));
    std::vector<uint8_t> request_frame = ipp::BuildBinaryFrame(request);

    // Send request.
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = server_url_;
    resource_request->method = "POST";
    resource_request->load_flags =
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    // TODO(pawliczek): create a traffic annotation for printing network traffic
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), kServerPrintersFetcherNetworkTag);
    std::string request_body(request_frame.begin(), request_frame.end());
    simple_url_loader_->AttachStringForUpload(request_body, "application/ipp");
    simple_url_loader_->DownloadAsStream(
        network::SharedURLLoaderFactory::Create(std::move(url_loader_factory))
            .get(),
        this);
  }

  // Posts a response with a list of printers.
  void PostResponse(std::vector<PrinterDetector::DetectedPrinter>&& printers) {
    task_runner_for_callback_->PostNonNestableTask(
        FROM_HERE, base::BindOnce(callback_, owner_, server_url_, printers));
  }

  // Set an object |printer| to represent a server printer with a name |name|.
  // The printer is provided by the current print server.
  void InitializePrinter(chromeos::Printer* printer, const std::string& name) {
    // All server printers are configured with IPP Everywhere.
    printer->mutable_ppd_reference()->autoconf = true;

    // As a display name we use the name fetched from the print server.
    printer->set_display_name(name);

    // Build printer's URL: we have to convert http/https to ipp/ipps because
    // some third-party components require ipp schema. E.g.:
    // * http://myprinter:123/abc =>  ipp://myprinter:123/abc
    // * http://myprinter/abc     =>  ipp://myprinter:80/abc
    // * https://myprinter/abc    =>  ipps://myprinter:443/abc
    chromeos::Uri url;
    if (server_url_.SchemeIs("https")) {
      url.SetScheme("ipps");
    } else {
      url.SetScheme("ipp");
    }
    url.SetHostEncoded(server_url_.HostNoBrackets());
    url.SetPort(server_url_.EffectiveIntPort());
    // Save the server URI.
    printer->set_print_server_uri(url.GetNormalized());
    // Complete building the printer's URI.
    url.SetPath({"printers", name});
    printer->SetUri(url);
    printer->set_id(ServerPrinterId(url.GetNormalized()));
  }

  raw_ptr<const ServerPrintersFetcher, DanglingUntriaged> owner_;
  const GURL server_url_;
  const std::string server_name_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_for_callback_;
  ServerPrintersFetcher::OnPrintersFetchedCallback callback_;

  // Raw payload of the HTTP response.
  std::vector<uint8_t> response_;

  PrintServerQueryResult last_error_ = PrintServerQueryResult::kNoErrors;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  SEQUENCE_CHECKER(sequence_checker_);
};

ServerPrintersFetcher::ServerPrintersFetcher(Profile* profile,
                                             const GURL& server_url,
                                             const std::string& server_name,
                                             OnPrintersFetchedCallback cb)
    : pim_(new PrivateImplementation(this,
                                     profile,
                                     server_url,
                                     server_name,
                                     std::move(cb)),
           PimDeleter()) {}

void ServerPrintersFetcher::PimDeleter::operator()(
    PrivateImplementation* pim) const {
  pim->Delete();
}

ServerPrintersFetcher::~ServerPrintersFetcher() = default;

PrintServerQueryResult ServerPrintersFetcher::GetLastError() const {
  return pim_->last_error();
}

}  // namespace ash
