// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/download_protection/document_analysis_service.h"
#include "chrome/browser/safe_browsing/download_protection/file_analyzer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/document_analyzer_results.h"
#include "chrome/services/file_util/document_analysis_service.h"
#include "chrome/test/base/in_process_browser_test.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#include "chrome/services/file_util/public/cpp/sandboxed_document_analyzer.h"
#endif

#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class FileAnalyzerBrowserTest : public InProcessBrowserTest {
 public:
  void AnalyzeDocument(const base::FilePath& file_path,
                       safe_browsing::DocumentAnalyzerResults* results) {
    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    scoped_refptr<SandboxedDocumentAnalyzer> analyzer(
        new SandboxedDocumentAnalyzer(file_path, file_path,
                                      results_getter.GetCallback(),
                                      LaunchDocumentAnalysisService()));
    analyzer->Start();
    run_loop.Run();
  }

  base::FilePath GetFilePath(const char* file_name) {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    return test_data.AppendASCII("safe_browsing")
        .AppendASCII("documents")
        .AppendASCII(file_name);
  }

 private:
  // Helper to provide a SandboxedDocumentAnalyzer::ResultCallback that
  // stores the analysis results and then runs the done closure.
  class ResultsGetter {
    using DocumentAnalyzerResults = safe_browsing::DocumentAnalyzerResults;

   public:
    ResultsGetter(base::OnceClosure done, DocumentAnalyzerResults* results)
        : done_closure_(std::move(done)), results_(results) {}

    SandboxedDocumentAnalyzer::ResultCallback GetCallback() {
      return base::BindOnce(&ResultsGetter::ResultsCallback,
                            base::Unretained(this));
    }

    ResultsGetter(const ResultsGetter&) = delete;
    ResultsGetter& operator=(const ResultsGetter&) = delete;

   private:
    void ResultsCallback(const DocumentAnalyzerResults& results) {
      *results_ = results;
      std::move(done_closure_).Run();
    }

    base::OnceClosure done_closure_;
    raw_ptr<DocumentAnalyzerResults> results_;
  };
};

IN_PROC_BROWSER_TEST_F(FileAnalyzerBrowserTest, AnalyzeDocumentWithMacros) {
  base::FilePath path;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("doc_containing_macros.doc"));
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  safe_browsing::DocumentAnalyzerResults results;
  AnalyzeDocument(path, &results);

  EXPECT_TRUE(results.success);
  EXPECT_EQ(ClientDownloadRequest::DocumentProcessingInfo::OK,
            results.error_code);
  EXPECT_TRUE(results.error_message.empty());
  EXPECT_TRUE(results.has_macros);
}

}  // namespace safe_browsing
