// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/server_printers_fetcher.h"

#include <string>

#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/device_event_log/device_event_log.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "third_party/libipp/libipp/ipp.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

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
                        const GURL& server_url,
                        const std::string& server_name,
                        ServerPrintersFetcher::OnPrintersFetchedCallback cb)
      : owner_(owner),
        server_url_(server_url),
        server_name_(server_name),
        callback_(std::move(cb)),
        task_runner_(base::CreateSequencedTaskRunner(
            {base::ThreadPool(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    CHECK(base::SequencedTaskRunnerHandle::IsSet());
    task_runner_for_callback_ = base::SequencedTaskRunnerHandle::Get();
    // Post task to execute.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&PrivateImplementation::SendQuery,
                                          base::Unretained(this)));
  }

  ~PrivateImplementation() override = default;

  // Schedule the given object for deletion. May be called from any
  // sequence/thread.
  void Delete() { task_runner_->DeleteSoon(FROM_HERE, this); }

  // Implementation of network::SimpleURLLoaderStreamConsumer.
  void OnDataReceived(base::StringPiece part_of_payload,
                      base::OnceClosure resume) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    response_.append(part_of_payload.begin(), part_of_payload.end());
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
    std::vector<uint8_t> data(response_.begin(), response_.end());
    ipp::Client client;
    ipp::Response_CUPS_Get_Printers response;
    if (!client.ReadResponseFrameFrom(data) ||
        !client.ParseResponseAndSaveTo(&response)) {
      // Parser has failed. Dump errors to the log.
      std::string message = "Cannot parse response from the print server " +
                            server_name_ + ". Parser log:";
      for (const auto& entry : client.GetErrorLog()) {
        message += "\n * " + entry.message;
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
    std::vector<PrinterDetector::DetectedPrinter> printers(
        response.printer_attributes.GetSize());
    for (size_t i = 0; i < printers.size(); ++i) {
      const std::string& name =
          response.printer_attributes[i].printer_name.Get().value;
      InitializePrinter(&(printers[i].printer), name);
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
  void SendQuery() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Preparation of the IPP frame.
    ipp::Request_CUPS_Get_Printers request;
    request.operation_attributes.requested_attributes.Set(
        {ipp::E_requested_attributes::printer_description});
    ipp::Client client;
    client.BuildRequestFrom(&request);
    std::vector<uint8_t> request_frame;
    client.WriteRequestFrameTo(&request_frame);

    // Send request.
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = server_url_;
    resource_request->method = "POST";
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                        "application/ipp");
    resource_request->load_flags =
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
    resource_request->request_body =
        network::ResourceRequestBody::CreateFromBytes(
            reinterpret_cast<char*>(request_frame.data()),
            request_frame.size());
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    // TODO(pawliczek): create a traffic annotation for printing network traffic
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), MISSING_TRAFFIC_ANNOTATION);
    network::mojom::URLLoaderFactory* loader_factory =
        g_browser_process->system_network_context_manager()
            ->GetURLLoaderFactory();
    simple_url_loader_->DownloadAsStream(loader_factory, this);
  }

  // Posts a response with a list of printers.
  void PostResponse(std::vector<PrinterDetector::DetectedPrinter>&& printers) {
    task_runner_for_callback_->PostNonNestableTask(
        FROM_HERE, base::BindOnce(callback_, owner_, server_url_, printers));
  }

  // Set an object |printer| to represent a server printer with a name |name|.
  // The printer is provided by the current print server.
  void InitializePrinter(Printer* printer, const std::string& name) {
    // All server printers are configured with IPP Everywhere.
    printer->mutable_ppd_reference()->autoconf = true;

    // As a display name we use the name fetched from the print server.
    printer->set_display_name(name);

    // Build printer's URL: we have to convert http/https to ipp/ipps because
    // some third-party components require ipp schema. E.g.:
    // * http://myprinter:123/abc =>  ipp://myprinter:123/abc
    // * http://myprinter/abc     =>  ipp://myprinter:80/abc
    // * https://myprinter/abc    =>  ipps://myprinter:443/abc
    std::string url = "ipp";
    if (server_url_.SchemeIs("https"))
      url += "s";
    url += "://";
    url += server_url_.HostNoBrackets();
    url += ":";
    url += base::NumberToString(server_url_.EffectiveIntPort());
    url += "/printers/" + name;
    printer->set_uri(url);
    printer->set_id(ServerPrinterId(url));
  }

  const ServerPrintersFetcher* owner_;
  const GURL server_url_;
  const std::string server_name_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_for_callback_;
  ServerPrintersFetcher::OnPrintersFetchedCallback callback_;

  // Raw payload of the HTTP response.
  std::string response_;

  PrintServerQueryResult last_error_ = PrintServerQueryResult::kNoErrors;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(PrivateImplementation);
};

ServerPrintersFetcher::ServerPrintersFetcher(const GURL& server_url,
                                             const std::string& server_name,
                                             OnPrintersFetchedCallback cb)
    : pim_(new PrivateImplementation(this,
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

}  // namespace chromeos
