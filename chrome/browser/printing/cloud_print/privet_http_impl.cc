// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_http_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/printing/cloud_print/privet_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "components/cloud_devices/common/printer_description.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"
#include "ui/gfx/text_elider.h"
#endif  // ENABLE_PRINT_PREVIEW

namespace cloud_print {

namespace {

const char kUrlPlaceHolder[] = "http://host/";
const char kPrivetRegisterActionArgName[] = "action";
const char kPrivetRegisterUserArgName[] = "user";

const int kPrivetCancelationTimeoutSeconds = 3;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
const char kPrivetURLKeyUserName[] = "user_name";
const char kPrivetURLKeyClientName[] = "client_name";
const char kPrivetURLKeyJobname[] = "job_name";
const char kPrivetURLValueClientName[] = "Chrome";

const char kPrivetContentTypePDF[] = "application/pdf";
const char kPrivetContentTypePWGRaster[] = "image/pwg-raster";
const char kPrivetContentTypeAny[] = "*/*";

const char kPrivetKeyJobID[] = "job_id";

const int kPrivetLocalPrintMaxRetries = 2;
const int kPrivetLocalPrintDefaultTimeout = 5;

const size_t kPrivetLocalPrintMaxJobNameLength = 64;
#endif  // ENABLE_PRINT_PREVIEW

GURL CreatePrivetURL(const std::string& path) {
  GURL url(kUrlPlaceHolder);
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

GURL CreatePrivetRegisterURL(const std::string& action,
                             const std::string& user) {
  GURL url = CreatePrivetURL(kPrivetRegisterPath);
  url = net::AppendQueryParameter(url, kPrivetRegisterActionArgName, action);
  return net::AppendQueryParameter(url, kPrivetRegisterUserArgName, user);
}

GURL CreatePrivetParamURL(const std::string& path,
                          const std::string& query_params) {
  GURL url(kUrlPlaceHolder);
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  if (!query_params.empty()) {
    replacements.SetQueryStr(query_params);
  }
  return url.ReplaceComponents(replacements);
}

}  // namespace

PrivetInfoOperationImpl::PrivetInfoOperationImpl(
    PrivetHTTPClient* privet_client,
    PrivetJSONOperation::ResultCallback callback)
    : privet_client_(privet_client), callback_(std::move(callback)) {}

PrivetInfoOperationImpl::~PrivetInfoOperationImpl() {
}

void PrivetInfoOperationImpl::Start() {
  url_loader_ = privet_client_->CreateURLLoader(
      CreatePrivetURL(kPrivetInfoPath), "GET", this);

  url_loader_->DoNotRetryOnTransientError();
  url_loader_->SendEmptyPrivetToken();

  url_loader_->Start();
}

PrivetHTTPClient* PrivetInfoOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetInfoOperationImpl::OnError(int response_code,
                                      PrivetURLLoader::ErrorType error) {
  if (callback_)
    std::move(callback_).Run(nullptr);
}

void PrivetInfoOperationImpl::OnParsedJson(int response_code,
                                           const base::DictionaryValue& value,
                                           bool has_error) {
  if (callback_)
    std::move(callback_).Run(&value);
}

// static
bool PrivetRegisterOperationImpl::run_tasks_immediately_for_testing_ = false;

PrivetRegisterOperationImpl::RunTasksImmediatelyForTesting::
    RunTasksImmediatelyForTesting() {
  DCHECK(!run_tasks_immediately_for_testing_);
  run_tasks_immediately_for_testing_ = true;
}

PrivetRegisterOperationImpl::RunTasksImmediatelyForTesting::
    ~RunTasksImmediatelyForTesting() {
  DCHECK(run_tasks_immediately_for_testing_);
  run_tasks_immediately_for_testing_ = false;
}

PrivetRegisterOperationImpl::PrivetRegisterOperationImpl(
    PrivetHTTPClient* privet_client,
    const std::string& user,
    PrivetRegisterOperation::Delegate* delegate)
    : user_(user), delegate_(delegate), privet_client_(privet_client) {}

PrivetRegisterOperationImpl::~PrivetRegisterOperationImpl() {
}

void PrivetRegisterOperationImpl::Start() {
  ongoing_ = true;
  next_response_handler_ = base::BindOnce(
      &PrivetRegisterOperationImpl::StartResponse, base::Unretained(this));
  SendRequest(kPrivetActionStart);
}

void PrivetRegisterOperationImpl::Cancel() {
  url_loader_.reset();

  if (!ongoing_)
    return;

  int delay_seconds =
      run_tasks_immediately_for_testing_ ? 0 : kPrivetCancelationTimeoutSeconds;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrivetRegisterOperationImpl::Cancelation::Cleanup,
                     base::Owned(new Cancelation(privet_client_, user_))),
      base::TimeDelta::FromSeconds(delay_seconds));
  ongoing_ = false;
}

void PrivetRegisterOperationImpl::CompleteRegistration() {
  next_response_handler_ = base::BindOnce(
      &PrivetRegisterOperationImpl::CompleteResponse, base::Unretained(this));
  SendRequest(kPrivetActionComplete);
}

PrivetHTTPClient* PrivetRegisterOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetRegisterOperationImpl::OnError(int response_code,
                                          PrivetURLLoader::ErrorType error) {
  ongoing_ = false;
  int visible_http_code = -1;
  FailureReason reason = FAILURE_NETWORK;

  if (error == PrivetURLLoader::RESPONSE_CODE_ERROR) {
    visible_http_code = response_code;
    reason = FAILURE_HTTP_ERROR;
  } else if (error == PrivetURLLoader::JSON_PARSE_ERROR) {
    reason = FAILURE_MALFORMED_RESPONSE;
  } else if (error == PrivetURLLoader::TOKEN_ERROR) {
    reason = FAILURE_TOKEN;
  } else if (error == PrivetURLLoader::UNKNOWN_ERROR) {
    reason = FAILURE_UNKNOWN;
  }

  delegate_->OnPrivetRegisterError(this, current_action_, reason,
                                   visible_http_code, nullptr);
}

void PrivetRegisterOperationImpl::OnParsedJson(
    int response_code,
    const base::DictionaryValue& value,
    bool has_error) {
  if (has_error) {
    std::string error;
    value.GetString(kPrivetKeyError, &error);

    ongoing_ = false;
    delegate_->OnPrivetRegisterError(this, current_action_, FAILURE_JSON_ERROR,
                                     response_code, &value);
    return;
  }

  // TODO(noamsml): Match the user&action with the user&action in the object,
  // and fail if different.
  std::move(next_response_handler_).Run(value);
}

void PrivetRegisterOperationImpl::OnNeedPrivetToken(
    PrivetURLLoader::TokenCallback callback) {
  privet_client_->RefreshPrivetToken(std::move(callback));
}

void PrivetRegisterOperationImpl::SendRequest(const std::string& action) {
  current_action_ = action;
  url_loader_ = privet_client_->CreateURLLoader(
      CreatePrivetRegisterURL(action, user_), "POST", this);
  url_loader_->Start();
}

void PrivetRegisterOperationImpl::StartResponse(
    const base::DictionaryValue& value) {
  next_response_handler_ =
      base::BindOnce(&PrivetRegisterOperationImpl::GetClaimTokenResponse,
                     base::Unretained(this));

  SendRequest(kPrivetActionGetClaimToken);
}

void PrivetRegisterOperationImpl::GetClaimTokenResponse(
    const base::DictionaryValue& value) {
  std::string claim_url;
  std::string claim_token;
  bool got_url = value.GetString(kPrivetKeyClaimURL, &claim_url);
  bool got_token = value.GetString(kPrivetKeyClaimToken, &claim_token);
  if (got_url || got_token) {
    delegate_->OnPrivetRegisterClaimToken(this, claim_token, GURL(claim_url));
  } else {
    delegate_->OnPrivetRegisterError(this, current_action_,
                                     FAILURE_MALFORMED_RESPONSE, -1, nullptr);
  }
}

void PrivetRegisterOperationImpl::CompleteResponse(
    const base::DictionaryValue& value) {
  std::string id;
  value.GetString(kPrivetKeyDeviceID, &id);
  ongoing_ = false;
  expected_id_ = id;
  StartInfoOperation();
}

void PrivetRegisterOperationImpl::OnPrivetInfoDone(
    const base::DictionaryValue* value) {
  // TODO(noamsml): Simplify error case and depracate HTTP error value in
  // OnPrivetRegisterError.
  if (!value) {
    delegate_->OnPrivetRegisterError(this, kPrivetActionNameInfo,
                                     FAILURE_NETWORK, -1, nullptr);
    return;
  }

  if (!value->HasKey(kPrivetInfoKeyID)) {
    if (value->HasKey(kPrivetKeyError)) {
      delegate_->OnPrivetRegisterError(this,
                                       kPrivetActionNameInfo,
                                        FAILURE_JSON_ERROR,
                                       -1,
                                       value);
    } else {
      delegate_->OnPrivetRegisterError(this, kPrivetActionNameInfo,
                                       FAILURE_MALFORMED_RESPONSE, -1, nullptr);
    }
    return;
  }

  std::string id;

  if (!value->GetString(kPrivetInfoKeyID, &id) ||
      id != expected_id_) {
    delegate_->OnPrivetRegisterError(this, kPrivetActionNameInfo,
                                     FAILURE_MALFORMED_RESPONSE, -1, nullptr);
  } else {
    delegate_->OnPrivetRegisterDone(this, id);
  }
}

void PrivetRegisterOperationImpl::StartInfoOperation() {
  info_operation_ = privet_client_->CreateInfoOperation(base::BindOnce(
      &PrivetRegisterOperationImpl::OnPrivetInfoDone, base::Unretained(this)));
  info_operation_->Start();
}

PrivetRegisterOperationImpl::Cancelation::Cancelation(
    PrivetHTTPClient* privet_client,
    const std::string& user) {
  url_loader_ = privet_client->CreateURLLoader(
      CreatePrivetRegisterURL(kPrivetActionCancel, user), "POST", this);
  url_loader_->DoNotRetryOnTransientError();
  url_loader_->Start();
}

PrivetRegisterOperationImpl::Cancelation::~Cancelation() {
}

void PrivetRegisterOperationImpl::Cancelation::OnError(
    int response_code,
    PrivetURLLoader::ErrorType error) {}

void PrivetRegisterOperationImpl::Cancelation::OnParsedJson(
    int response_code,
    const base::DictionaryValue& value,
    bool has_error) {}

void PrivetRegisterOperationImpl::Cancelation::Cleanup() {
  // Nothing needs to be done, as base::Owned will delete this object,
  // this callback is just here to pass ownership of the Cancelation to
  // the message loop.
}

PrivetJSONOperationImpl::PrivetJSONOperationImpl(
    PrivetHTTPClient* privet_client,
    const std::string& path,
    const std::string& query_params,
    PrivetJSONOperation::ResultCallback callback)
    : privet_client_(privet_client),
      path_(path),
      query_params_(query_params),
      callback_(std::move(callback)) {}

PrivetJSONOperationImpl::~PrivetJSONOperationImpl() {
}

void PrivetJSONOperationImpl::Start() {
  url_loader_ = privet_client_->CreateURLLoader(
      CreatePrivetParamURL(path_, query_params_), "GET", this);
  url_loader_->DoNotRetryOnTransientError();
  url_loader_->Start();
}

PrivetHTTPClient* PrivetJSONOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetJSONOperationImpl::OnError(int response_code,
                                      PrivetURLLoader::ErrorType error) {
  if (callback_)
    std::move(callback_).Run(nullptr);
}

void PrivetJSONOperationImpl::OnParsedJson(int response_code,
                                           const base::DictionaryValue& value,
                                           bool has_error) {
  if (callback_)
    std::move(callback_).Run(&value);
}

void PrivetJSONOperationImpl::OnNeedPrivetToken(
    PrivetURLLoader::TokenCallback callback) {
  privet_client_->RefreshPrivetToken(std::move(callback));
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// static
bool PrivetLocalPrintOperationImpl::run_tasks_immediately_for_testing_ = false;

PrivetLocalPrintOperationImpl::RunTasksImmediatelyForTesting::
    RunTasksImmediatelyForTesting() {
  DCHECK(!run_tasks_immediately_for_testing_);
  run_tasks_immediately_for_testing_ = true;
}

PrivetLocalPrintOperationImpl::RunTasksImmediatelyForTesting::
    ~RunTasksImmediatelyForTesting() {
  DCHECK(run_tasks_immediately_for_testing_);
  run_tasks_immediately_for_testing_ = false;
}

PrivetLocalPrintOperationImpl::PrivetLocalPrintOperationImpl(
    PrivetHTTPClient* privet_client,
    PrivetLocalPrintOperation::Delegate* delegate)
    : privet_client_(privet_client), delegate_(delegate) {}

PrivetLocalPrintOperationImpl::~PrivetLocalPrintOperationImpl() {
}

void PrivetLocalPrintOperationImpl::Start() {
  DCHECK(!started_);

  // We need to get the /info response so we can know which APIs are available.
  // TODO(noamsml): Use cached info when available.
  info_operation_ = privet_client_->CreateInfoOperation(
      base::BindOnce(&PrivetLocalPrintOperationImpl::OnPrivetInfoDone,
                     weak_factory_.GetWeakPtr()));
  info_operation_->Start();
  started_ = true;
}

void PrivetLocalPrintOperationImpl::OnPrivetInfoDone(
    const base::DictionaryValue* value) {
  if (!value || value->HasKey(kPrivetKeyError)) {
    delegate_->OnPrivetPrintingError(this, -1);
    return;
  }

  has_extended_workflow_ = false;
  bool has_printing = false;
  const base::Value* api_list =
      value->FindKeyOfType(kPrivetInfoKeyAPIList, base::Value::Type::LIST);
  if (api_list) {
    for (const auto& api : api_list->GetList()) {
      if (!api.is_string())
        continue;

      const std::string& api_str = api.GetString();
      if (!has_printing && api_str == kPrivetSubmitdocPath)
        has_printing = true;
      else if (!has_extended_workflow_ && api_str == kPrivetCreatejobPath)
        has_extended_workflow_ = true;

      if (has_printing && has_extended_workflow_)
        break;
    }
  }

  if (!has_printing) {
    delegate_->OnPrivetPrintingError(this, -1);
    return;
  }

  StartInitialRequest();
}

void PrivetLocalPrintOperationImpl::StartInitialRequest() {
  cloud_devices::printer::ContentTypesCapability content_types;
  if (content_types.LoadFrom(capabilities_)) {
    use_pdf_ = content_types.Contains(kPrivetContentTypePDF) ||
               content_types.Contains(kPrivetContentTypeAny);
  } else {
    use_pdf_ = false;
  }

  if (use_pdf_) {
    StartPrinting();
  } else {
    StartConvertToPWG();
  }
}

void PrivetLocalPrintOperationImpl::DoCreatejob() {
  current_response_ =
      base::BindOnce(&PrivetLocalPrintOperationImpl::OnCreatejobResponse,
                     weak_factory_.GetWeakPtr());

  url_loader_ = privet_client_->CreateURLLoader(
      CreatePrivetURL(kPrivetCreatejobPath), "POST", this);
  url_loader_->SetUploadData(kContentTypeJSON, ticket_.ToString());

  url_loader_->Start();
}

void PrivetLocalPrintOperationImpl::DoSubmitdoc() {
  current_response_ =
      base::BindOnce(&PrivetLocalPrintOperationImpl::OnSubmitdocResponse,
                     weak_factory_.GetWeakPtr());

  GURL url = CreatePrivetURL(kPrivetSubmitdocPath);

  url = net::AppendQueryParameter(url,
                                  kPrivetURLKeyClientName,
                                  kPrivetURLValueClientName);

  if (!user_.empty()) {
    url = net::AppendQueryParameter(url,
                                    kPrivetURLKeyUserName,
                                    user_);
  }

  base::string16 shortened_jobname;
  gfx::ElideString(base::UTF8ToUTF16(jobname_),
                   kPrivetLocalPrintMaxJobNameLength,
                   &shortened_jobname);

  if (!jobname_.empty()) {
    url = net::AppendQueryParameter(
        url, kPrivetURLKeyJobname, base::UTF16ToUTF8(shortened_jobname));
  }

  if (!jobid_.empty()) {
    url = net::AppendQueryParameter(url,
                                    kPrivetKeyJobID,
                                    jobid_);
  }

  url_loader_ = privet_client_->CreateURLLoader(url, "POST", this);

  std::string data_str(reinterpret_cast<const char*>(data_->front()),
                       data_->size());
  url_loader_->SetUploadData(
      use_pdf_ ? kPrivetContentTypePDF : kPrivetContentTypePWGRaster, data_str);
  url_loader_->Start();
}

void PrivetLocalPrintOperationImpl::StartPrinting() {
  if (has_extended_workflow_ && jobid_.empty()) {
    DoCreatejob();
  } else {
    DoSubmitdoc();
  }
}

void PrivetLocalPrintOperationImpl::StartConvertToPWG() {
  using printing::PwgRasterConverter;
  if (!pwg_raster_converter_)
    pwg_raster_converter_ = PwgRasterConverter::CreateDefault();

  printing::PwgRasterSettings bitmap_settings =
      PwgRasterConverter::GetBitmapSettings(capabilities_, ticket_);
  pwg_raster_converter_->Start(
      data_.get(),
      PwgRasterConverter::GetConversionSettings(capabilities_, page_size_,
                                                bitmap_settings.use_color),
      bitmap_settings,
      base::BindOnce(&PrivetLocalPrintOperationImpl::OnPWGRasterConverted,
                     weak_factory_.GetWeakPtr()));
}

void PrivetLocalPrintOperationImpl::OnSubmitdocResponse(
    bool has_error,
    const base::DictionaryValue* value) {
  std::string error;
  // This error is only relevant in the case of extended workflow:
  // If the print job ID is invalid, retry createjob and submitdoc,
  // rather than simply retrying the current request.
  if (has_error && value->GetString(kPrivetKeyError, &error)) {
    if (has_extended_workflow_ &&
        error == kPrivetErrorInvalidPrintJob &&
        invalid_job_retries_ < kPrivetLocalPrintMaxRetries) {
      invalid_job_retries_++;

      int timeout = kPrivetLocalPrintDefaultTimeout;
      value->GetInteger(kPrivetKeyTimeout, &timeout);

      double random_scaling_factor =
          1 + base::RandDouble() * kPrivetMaximumTimeRandomAddition;

      timeout =
          run_tasks_immediately_for_testing_
              ? 0
              : std::max(static_cast<int>(timeout * random_scaling_factor),
                         kPrivetMinimumTimeout);
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PrivetLocalPrintOperationImpl::DoCreatejob,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(timeout));
    } else if (use_pdf_ && error == kPrivetErrorInvalidDocumentType) {
      use_pdf_ = false;
      StartConvertToPWG();
    } else {
      delegate_->OnPrivetPrintingError(this, 200);
    }

    return;
  }

  // If we've gotten this far, there are no errors, so we've effectively
  // succeeded.
  delegate_->OnPrivetPrintingDone(this);
}

void PrivetLocalPrintOperationImpl::OnCreatejobResponse(
    bool has_error,
    const base::DictionaryValue* value) {
  if (has_error) {
    delegate_->OnPrivetPrintingError(this, 200);
    return;
  }

  // Try to get job ID from value. If not, |jobid_| will be empty and we will
  // use simple printing.
  value->GetString(kPrivetKeyJobID, &jobid_);
  DoSubmitdoc();
}

void PrivetLocalPrintOperationImpl::OnPWGRasterConverted(
    base::ReadOnlySharedMemoryRegion pwg_region) {
  auto data =
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(pwg_region);
  if (!data) {
    delegate_->OnPrivetPrintingError(this, -1);
    return;
  }

  data_ = data;
  StartPrinting();
}

PrivetHTTPClient* PrivetLocalPrintOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetLocalPrintOperationImpl::OnError(int response_code,
                                            PrivetURLLoader::ErrorType error) {
  delegate_->OnPrivetPrintingError(this, -1);
}

void PrivetLocalPrintOperationImpl::OnParsedJson(
    int response_code,
    const base::DictionaryValue& value,
    bool has_error) {
  DCHECK(current_response_);
  std::move(current_response_).Run(has_error, &value);
}

void PrivetLocalPrintOperationImpl::OnNeedPrivetToken(
    PrivetURLLoader::TokenCallback callback) {
  privet_client_->RefreshPrivetToken(std::move(callback));
}

void PrivetLocalPrintOperationImpl::SetData(
    scoped_refptr<base::RefCountedMemory> data) {
  DCHECK(!started_);
  data_ = data;
}

void PrivetLocalPrintOperationImpl::SetTicket(base::Value ticket) {
  DCHECK(!started_);
  ticket_.InitFromValue(std::move(ticket));
}

void PrivetLocalPrintOperationImpl::SetCapabilities(
    const std::string& capabilities) {
  DCHECK(!started_);
  capabilities_.InitFromString(capabilities);
}

void PrivetLocalPrintOperationImpl::SetUsername(const std::string& user) {
  DCHECK(!started_);
  user_ = user;
}

void PrivetLocalPrintOperationImpl::SetJobname(const std::string& jobname) {
  DCHECK(!started_);
  jobname_ = jobname;
}

void PrivetLocalPrintOperationImpl::SetPageSize(const gfx::Size& page_size) {
  DCHECK(!started_);
  page_size_ = page_size;
}

void PrivetLocalPrintOperationImpl::SetPwgRasterConverterForTesting(
    std::unique_ptr<printing::PwgRasterConverter> pwg_raster_converter) {
  pwg_raster_converter_ = std::move(pwg_raster_converter);
}
#endif  // ENABLE_PRINT_PREVIEW

PrivetHTTPClientImpl::PrivetHTTPClientImpl(
    const std::string& name,
    const net::HostPortPair& host_port,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : name_(name),
      url_loader_factory_(url_loader_factory),
      host_port_(host_port) {
  DCHECK(url_loader_factory_);
}

PrivetHTTPClientImpl::~PrivetHTTPClientImpl() {
}

const std::string& PrivetHTTPClientImpl::GetName() {
  return name_;
}

std::unique_ptr<PrivetJSONOperation> PrivetHTTPClientImpl::CreateInfoOperation(
    PrivetJSONOperation::ResultCallback callback) {
  return std::make_unique<PrivetInfoOperationImpl>(this, std::move(callback));
}

std::unique_ptr<PrivetURLLoader> PrivetHTTPClientImpl::CreateURLLoader(
    const GURL& url,
    const std::string& request_type,
    PrivetURLLoader::Delegate* delegate) {
  GURL::Replacements replacements;
  std::string host = host_port_.HostForURL();
  replacements.SetHostStr(host);
  std::string port = base::NumberToString(host_port_.port());
  replacements.SetPortStr(port);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("privet_http_impl", R"(
        semantics {
          sender: "Cloud Print"
          description:
            "Cloud Print local printing uses these requests to query "
            "information from printers on local network and send print jobs to "
            "them."
          trigger:
            "Print Preview; New printer on network; chrome://devices/"
          data:
            "Printer information, settings and document for printing."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable background requests by 'Show "
            "notifications when new printers are detected on the network' in "
            "Chromium's settings under Advanced Settings, Google Cloud Print. "
            "User triggered requests, like from print preview or "
            "chrome://devices/ cannot be disabled."
          policy_exception_justification:
            "Not implemented, it's good to do so."
        })");
  return std::make_unique<PrivetURLLoader>(url.ReplaceComponents(replacements),
                                           request_type, url_loader_factory_,
                                           traffic_annotation, delegate);
}

void PrivetHTTPClientImpl::RefreshPrivetToken(
    PrivetURLLoader::TokenCallback callback) {
  token_callbacks_.push_back(std::move(callback));

  if (info_operation_)
    return;

  info_operation_ = CreateInfoOperation(base::BindOnce(
      &PrivetHTTPClientImpl::OnPrivetInfoDone, base::Unretained(this)));
  info_operation_->Start();
}

void PrivetHTTPClientImpl::OnPrivetInfoDone(
    const base::DictionaryValue* value) {
  info_operation_.reset();

  // If this does not succeed, token will be empty, and an empty string
  // is our sentinel value, since empty X-Privet-Tokens are not allowed.
  std::string token;
  if (value)
    value->GetString(kPrivetInfoKeyToken, &token);

  TokenCallbackVector token_callbacks;
  token_callbacks_.swap(token_callbacks);

  for (auto& callback : token_callbacks)
    std::move(callback).Run(token);
}

PrivetV1HTTPClientImpl::PrivetV1HTTPClientImpl(
    std::unique_ptr<PrivetHTTPClient> info_client)
    : info_client_(std::move(info_client)) {}

PrivetV1HTTPClientImpl::~PrivetV1HTTPClientImpl() {
}

const std::string& PrivetV1HTTPClientImpl::GetName() {
  return info_client_->GetName();
}

std::unique_ptr<PrivetJSONOperation>
PrivetV1HTTPClientImpl::CreateInfoOperation(
    PrivetJSONOperation::ResultCallback callback) {
  return info_client_->CreateInfoOperation(std::move(callback));
}

std::unique_ptr<PrivetRegisterOperation>
PrivetV1HTTPClientImpl::CreateRegisterOperation(
    const std::string& user,
    PrivetRegisterOperation::Delegate* delegate) {
  return std::make_unique<PrivetRegisterOperationImpl>(info_client_.get(), user,
                                                       delegate);
}

std::unique_ptr<PrivetJSONOperation>
PrivetV1HTTPClientImpl::CreateCapabilitiesOperation(
    PrivetJSONOperation::ResultCallback callback) {
  return std::make_unique<PrivetJSONOperationImpl>(
      info_client_.get(), kPrivetCapabilitiesPath, "", std::move(callback));
}

std::unique_ptr<PrivetLocalPrintOperation>
PrivetV1HTTPClientImpl::CreateLocalPrintOperation(
    PrivetLocalPrintOperation::Delegate* delegate) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  return std::make_unique<PrivetLocalPrintOperationImpl>(info_client_.get(),
                                                         delegate);
#else
  return nullptr;
#endif  // ENABLE_PRINT_PREVIEW
}

}  // namespace cloud_print
