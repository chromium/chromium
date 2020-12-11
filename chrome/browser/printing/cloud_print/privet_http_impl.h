// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_IMPL_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/printing/cloud_print/privet_http.h"
#include "chrome/browser/printing/cloud_print/privet_url_loader.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "printing/buildflags/buildflags.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class RefCountedMemory;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace cloud_print {

class PrivetHTTPClient;

class PrivetInfoOperationImpl : public PrivetJSONOperation,
                                public PrivetURLLoader::Delegate {
 public:
  PrivetInfoOperationImpl(PrivetHTTPClient* privet_client,
                          PrivetJSONOperation::ResultCallback callback);
  ~PrivetInfoOperationImpl() override;

  // PrivetJSONOperation:
  void Start() override;
  PrivetHTTPClient* GetHTTPClient() override;

  // PrivetURLLoader::Delegate:
  void OnError(int response_code, PrivetURLLoader::ErrorType error) override;
  void OnParsedJson(int response_code,
                    const base::DictionaryValue& value,
                    bool has_error) override;

 private:
  PrivetHTTPClient* const privet_client_;
  PrivetJSONOperation::ResultCallback callback_;
  std::unique_ptr<PrivetURLLoader> url_loader_;
};

class PrivetRegisterOperationImpl
    : public PrivetRegisterOperation,
      public PrivetURLLoader::Delegate,
      public base::SupportsWeakPtr<PrivetRegisterOperationImpl> {
 public:
  PrivetRegisterOperationImpl(PrivetHTTPClient* privet_client,
                              const std::string& user,
                              PrivetRegisterOperation::Delegate* delegate);
  ~PrivetRegisterOperationImpl() override;

  // PrivetRegisterOperation:
  void Start() override;
  void Cancel() override;
  void CompleteRegistration() override;
  PrivetHTTPClient* GetHTTPClient() override;

  // PrivetURLLoader::Delegate:
  void OnError(int response_code, PrivetURLLoader::ErrorType error) override;
  void OnParsedJson(int response_code,
                    const base::DictionaryValue& value,
                    bool has_error) override;
  void OnNeedPrivetToken(PrivetURLLoader::TokenCallback callback) override;

  // Used in test to skip delays when posting tasks for cancellation.
  class RunTasksImmediatelyForTesting final {
   public:
    RunTasksImmediatelyForTesting();
    ~RunTasksImmediatelyForTesting();
  };

 private:
  class Cancelation : public PrivetURLLoader::Delegate {
   public:
    Cancelation(PrivetHTTPClient* privet_client, const std::string& user);
    ~Cancelation() override;

    // PrivetURLLoader::Delegate:
    void OnError(int response_code, PrivetURLLoader::ErrorType error) override;
    void OnParsedJson(int response_code,
                      const base::DictionaryValue& value,
                      bool has_error) override;

    void Cleanup();

   private:
    std::unique_ptr<PrivetURLLoader> url_loader_;
  };

  // Arguments is JSON value from request.
  using ResponseHandler =
      base::OnceCallback<void(const base::DictionaryValue&)>;

  void StartInfoOperation();
  void OnPrivetInfoDone(const base::DictionaryValue* value);

  void StartResponse(const base::DictionaryValue& value);
  void GetClaimTokenResponse(const base::DictionaryValue& value);
  void CompleteResponse(const base::DictionaryValue& value);

  void SendRequest(const std::string& action);

  const std::string user_;
  std::string current_action_;
  std::unique_ptr<PrivetURLLoader> url_loader_;
  PrivetRegisterOperation::Delegate* const delegate_;
  PrivetHTTPClient* const privet_client_;
  ResponseHandler next_response_handler_;
  // Required to ensure destroying completed register operations doesn't cause
  // extraneous cancelations.
  bool ongoing_ = false;

  std::unique_ptr<PrivetJSONOperation> info_operation_;
  std::string expected_id_;

  static bool run_tasks_immediately_for_testing_;
};

class PrivetJSONOperationImpl : public PrivetJSONOperation,
                                public PrivetURLLoader::Delegate {
 public:
  PrivetJSONOperationImpl(PrivetHTTPClient* privet_client,
                          const std::string& path,
                          const std::string& query_params,
                          PrivetJSONOperation::ResultCallback callback);
  ~PrivetJSONOperationImpl() override;

  // PrivetJSONOperation:
  void Start() override;
  PrivetHTTPClient* GetHTTPClient() override;

  // PrivetURLLoader::Delegate:
  void OnError(int response_code, PrivetURLLoader::ErrorType error) override;
  void OnParsedJson(int response_code,
                    const base::DictionaryValue& value,
                    bool has_error) override;
  void OnNeedPrivetToken(PrivetURLLoader::TokenCallback callback) override;

 private:
  PrivetHTTPClient* const privet_client_;
  const std::string path_;
  const std::string query_params_;
  PrivetJSONOperation::ResultCallback callback_;

  std::unique_ptr<PrivetURLLoader> url_loader_;
};

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class PrivetLocalPrintOperationImpl : public PrivetLocalPrintOperation,
                                      public PrivetURLLoader::Delegate {
 public:
  PrivetLocalPrintOperationImpl(PrivetHTTPClient* privet_client,
                                PrivetLocalPrintOperation::Delegate* delegate);
  ~PrivetLocalPrintOperationImpl() override;

  // PrivetLocalPrintOperation:
  void Start() override;
  void SetData(scoped_refptr<base::RefCountedMemory> data) override;
  void SetTicket(base::Value ticket) override;
  void SetCapabilities(const std::string& capabilities) override;
  void SetUsername(const std::string& user) override;
  void SetJobname(const std::string& jobname) override;
  void SetPageSize(const gfx::Size& page_size) override;
  void SetPwgRasterConverterForTesting(
      std::unique_ptr<printing::PwgRasterConverter> pwg_raster_converter)
      override;
  PrivetHTTPClient* GetHTTPClient() override;

  // PrivetURLLoader::Delegate:
  void OnError(int response_code, PrivetURLLoader::ErrorType error) override;
  void OnParsedJson(int response_code,
                    const base::DictionaryValue& value,
                    bool has_error) override;
  void OnNeedPrivetToken(PrivetURLLoader::TokenCallback callback) override;

  // Used in test to skip delays when posting tasks for cancellation.
  class RunTasksImmediatelyForTesting final {
   public:
    RunTasksImmediatelyForTesting();
    ~RunTasksImmediatelyForTesting();
  };

 private:
  using ResponseCallback =
      base::OnceCallback<void(/*has_error=*/bool,
                              const base::DictionaryValue* value)>;

  void StartInitialRequest();
  void DoCreatejob();
  void DoSubmitdoc();

  void StartConvertToPWG();
  void StartPrinting();

  void OnPrivetInfoDone(const base::DictionaryValue* value);
  void OnSubmitdocResponse(bool has_error,
                           const base::DictionaryValue* value);
  void OnCreatejobResponse(bool has_error,
                           const base::DictionaryValue* value);
  void OnPWGRasterConverted(base::ReadOnlySharedMemoryRegion pwg_region);

  PrivetHTTPClient* const privet_client_;
  PrivetLocalPrintOperation::Delegate* const delegate_;

  ResponseCallback current_response_;

  cloud_devices::CloudDeviceDescription ticket_;
  cloud_devices::CloudDeviceDescription capabilities_;

  scoped_refptr<base::RefCountedMemory> data_;

  bool use_pdf_ = false;
  bool has_extended_workflow_ = false;
  bool started_ = false;
  gfx::Size page_size_;

  std::string user_;
  std::string jobname_;

  std::string jobid_;

  int invalid_job_retries_ = 0;

  std::unique_ptr<PrivetURLLoader> url_loader_;
  std::unique_ptr<PrivetJSONOperation> info_operation_;
  std::unique_ptr<printing::PwgRasterConverter> pwg_raster_converter_;

  base::WeakPtrFactory<PrivetLocalPrintOperationImpl> weak_factory_{this};

  static bool run_tasks_immediately_for_testing_;
};
#endif  // ENABLE_PRINT_PREVIEW

class PrivetHTTPClientImpl : public PrivetHTTPClient {
 public:
  PrivetHTTPClientImpl(
      const std::string& name,
      const net::HostPortPair& host_port,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PrivetHTTPClientImpl() override;

  // PrivetHTTPClient:
  const std::string& GetName() override;
  std::unique_ptr<PrivetJSONOperation> CreateInfoOperation(
      PrivetJSONOperation::ResultCallback callback) override;
  std::unique_ptr<PrivetURLLoader> CreateURLLoader(
      const GURL& url,
      const std::string& request_type,
      PrivetURLLoader::Delegate* delegate) override;
  void RefreshPrivetToken(
      PrivetURLLoader::TokenCallback token_callback) override;

 private:
  using TokenCallbackVector = std::vector<PrivetURLLoader::TokenCallback>;

  void OnPrivetInfoDone(const base::DictionaryValue* value);

  const std::string name_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const net::HostPortPair host_port_;

  std::unique_ptr<PrivetJSONOperation> info_operation_;
  TokenCallbackVector token_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PrivetHTTPClientImpl);
};

class PrivetV1HTTPClientImpl : public PrivetV1HTTPClient {
 public:
  explicit PrivetV1HTTPClientImpl(
      std::unique_ptr<PrivetHTTPClient> info_client);
  ~PrivetV1HTTPClientImpl() override;

  // PrivetV1HTTPClient:
  const std::string& GetName() override;
  std::unique_ptr<PrivetJSONOperation> CreateInfoOperation(
      PrivetJSONOperation::ResultCallback callback) override;
  std::unique_ptr<PrivetRegisterOperation> CreateRegisterOperation(
      const std::string& user,
      PrivetRegisterOperation::Delegate* delegate) override;
  std::unique_ptr<PrivetJSONOperation> CreateCapabilitiesOperation(
      PrivetJSONOperation::ResultCallback callback) override;
  std::unique_ptr<PrivetLocalPrintOperation> CreateLocalPrintOperation(
      PrivetLocalPrintOperation::Delegate* delegate) override;

 private:
  std::unique_ptr<PrivetHTTPClient> info_client_;

  DISALLOW_COPY_AND_ASSIGN(PrivetV1HTTPClientImpl);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_IMPL_H_
