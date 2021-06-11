// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_TEST_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_TEST_HELPER_H_

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"
#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class BoxUploader;

class BoxUploaderTestBase : public testing::Test {
 public:
  explicit BoxUploaderTestBase(base::FilePath::StringPieceType file_name =
                                   FILE_PATH_LITERAL("box_uploader_test.txt"));
  ~BoxUploaderTestBase() override;

  base::FilePath GetFilePath() const;

 protected:
  using State = download::DownloadItem::DownloadState;

  virtual void CreateTemporaryFile();
  void CreateTemporaryFileWithContent(std::string content);
  void InitFolderIdInPrefs(std::string folder_id);
  void InitUploader(BoxUploader* uploader);

  void AuthenticationRetry();
  void OnProgressUpdate(
      const download::DownloadItemRenameProgressUpdate& update);
  void OnUploaderFinished(bool success, const base::FilePath& final_name);

  // Add a mock response to http requests made to the url. Only the last
  // response added is used.
  void AddFetchResult(const std::string& url,
                      net::HttpStatusCode code,
                      std::string body = std::string());
  // Add multiple responses for the same url to be consumed in a sequence
  // (FIFO). Any response previously added via AddFetchResult() is overwritten.
  void AddSequentialFetchResult(const std::string& url,
                                net::HttpStatusCode code,
                                std::string body = std::string());
  void AddSequentialFetchResult(const std::string& url,
                                network::mojom::URLResponseHeadPtr head,
                                std::string body = std::string());

  // Use these wherever possible, instead of base::RunLoop().RunUntilIdle(),
  // which is flaky in multi-threaded environment.
  void RunWithQuitClosure();
  void Quit();

  DownloadItemForTest test_item_;

  // For uploader.TryTask().
  scoped_refptr<network::SharedURLLoaderFactory> url_factory_;

  // Updated/used in callbacks & checked in tests.
  int authentication_retry_{0};
  int progress_update_cb_called_{0};
  bool download_thread_cb_called_{false};
  bool upload_success_{false};
  base::FilePath validated_file_name_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;  // For url_factory_.
  // Decoder and TestingProfileManager must be declared after TaskEnvironment.
  data_decoder::test::InProcessDataDecoder decoder_;  // For parsing responses.
  TestingProfileManager profile_manager_;             // For prefs_.
  PrefService* prefs_;                                // For storing folder_id.

  // For RunWithQuitClosure() and Quit().
  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure quit_closure_;

  // Helper methods, struct, and member for AddSequentialFetchResult().
  void SetInterceptorForURLLoader(network::TestURLLoaderFactory::Interceptor);
  void SetNextResponseForURLLoader(const network::ResourceRequest& request);
  struct HttpResponse {
    HttpResponse(network::mojom::URLResponseHeadPtr head, std::string body);
    ~HttpResponse();
    HttpResponse(HttpResponse&&);

    network::mojom::URLResponseHeadPtr head_;
    std::string body_;
  };
  std::multimap<GURL, HttpResponse> responses_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_TEST_HELPER_H_
