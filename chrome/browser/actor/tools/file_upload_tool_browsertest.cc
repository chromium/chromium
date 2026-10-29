// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/file_picker_interceptor.h"
#include "chrome/browser/actor/tools/file_upload_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "url/gurl.h"

namespace actor {
namespace {

using ::content::EvalJs;
using ::content::GetDOMNodeId;
using ::optimization_guide::DocumentIdentifierUserData;

constexpr char kPageWithFileInput[] = "/actor/page_with_file_input.html";
constexpr char kPayloadPath[] = "/actor/upload_payload.txt";
constexpr char kPayloadContents[] = "actor upload payload\n";

// Host whose files are downloaded with the user's credentials. Any other host
// (e.g. example.com, bar.com) is not.
constexpr char kCredentialedHost[] = "foo.com";

// Evaluates to the contents of the first file handed to the page.
constexpr char kFirstFileContents[] =
    "files_selected.then(() => "
    "document.getElementById('fileinput').files[0].text())";

class NoopFileSelectListener : public content::FileSelectListener {
 public:
  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {}
  void FileSelectionCanceled() override {}

 private:
  ~NoopFileSelectListener() override = default;
};

class ActorFileUploadToolBrowserTest : public ActorToolsTest {
 public:
  ActorFileUploadToolBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kGlicActorFileUploadTool, {{"credentialed_hosts", kCredentialedHost}});
  }
  ~ActorFileUploadToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    // Provides /echoheader and /server-redirect.
    net::test_server::RegisterDefaultHandlers(&embedded_https_test_server());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 protected:
  // Navigates to the page with a file input and returns a request that uploads
  // `urls` into it.
  std::unique_ptr<ToolRequest> NavigateAndMakeUploadRequest(
      const std::vector<GURL>& urls) {
    std::vector<FileUploadSource> files;
    for (const GURL& file_url : urls) {
      files.emplace_back(file_url);
    }
    return NavigateAndMakeUploadRequestForSources(std::move(files));
  }

  // Navigates to the page with a file input and returns a request that uploads
  // `files` into it.
  std::unique_ptr<ToolRequest> NavigateAndMakeUploadRequestForSources(
      std::vector<FileUploadSource> files) {
    const GURL url =
        embedded_https_test_server().GetURL("example.com", kPageWithFileInput);
    if (!content::NavigateToURL(web_contents(), url)) {
      ADD_FAILURE() << "Failed to navigate to " << url;
      return nullptr;
    }

    std::optional<int> input_id =
        GetDOMNodeId(*main_frame(), "input#fileinput");
    if (!input_id) {
      ADD_FAILURE() << "Could not find the file input";
      return nullptr;
    }

    std::string document_identifier =
        *DocumentIdentifierUserData::GetDocumentIdentifier(
            main_frame()->GetGlobalFrameToken());
    PageTarget target = DomNode{.node_id = *input_id,
                                .document_identifier = document_identifier};

    return std::make_unique<FileUploadToolRequest>(
        ActorSurfaceHandle::From(active_tab()->GetHandle()), std::move(target),
        std::move(files));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// The file is downloaded, handed to the page in place of the file picker, and
// its contents are readable by the page.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest, UploadsFileFromUrl) {
  const GURL payload_url =
      embedded_https_test_server().GetURL("example.com", kPayloadPath);
  std::unique_ptr<ToolRequest> action =
      NavigateAndMakeUploadRequest({payload_url});
  ASSERT_TRUE(action);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // `files_selected` resolves once the input's change event has fired.
  EXPECT_EQ(base::StrCat({"upload_payload.txt|",
                          base::NumberToString(sizeof(kPayloadContents) - 1),
                          "|", kPayloadContents}),
            EvalJs(web_contents(), "files_selected").ExtractString());

  // The real path of the staged file must not be exposed to the page.
  EXPECT_EQ("", EvalJs(web_contents(),
                       "document.getElementById('fileinput').files[0].webkitRe"
                       "lativePath")
                    .ExtractString());
}

// Multiple files can be uploaded to a `multiple` input in one action.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest, UploadsMultipleFiles) {
  const GURL payload_url =
      embedded_https_test_server().GetURL("example.com", kPayloadPath);
  std::unique_ptr<ToolRequest> action =
      NavigateAndMakeUploadRequest({payload_url, payload_url});
  ASSERT_TRUE(action);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(2, EvalJs(web_contents(),
                      "files_selected.then(() => "
                      "document.getElementById('fileinput').files.length)")
                   .ExtractInt());
}

// A file name containing characters that are significant in URLs reaches the
// page unchanged: it is neither cut off at '#' nor decoded at '%', and the
// file's contents are still readable.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest,
                       PreservesUrlSignificantCharactersInFileName) {
  constexpr char kFileName[] = "Q&A #1 100%2F.txt";
  std::vector<FileUploadSource> files;
  files.emplace_back(
      embedded_https_test_server().GetURL("example.com", kPayloadPath),
      kFileName);
  std::unique_ptr<ToolRequest> action =
      NavigateAndMakeUploadRequestForSources(std::move(files));
  ASSERT_TRUE(action);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(base::StrCat({kFileName, "|",
                          base::NumberToString(sizeof(kPayloadContents) - 1),
                          "|", kPayloadContents}),
            EvalJs(web_contents(), "files_selected").ExtractString());
}

// The files are staged for the document holding the target, so an input in a
// cross-site iframe receives them.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest,
                       UploadsFileIntoSubframe) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL(
                          "example.com", "/actor/positioned_iframe.html")));
  ASSERT_TRUE(content::NavigateIframeToURL(
      web_contents(), "iframe",
      embedded_https_test_server().GetURL("bar.com", kPageWithFileInput)));
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame(), 0);
  ASSERT_TRUE(subframe);

  std::optional<int> input_id = GetDOMNodeId(*subframe, "input#fileinput");
  ASSERT_TRUE(input_id);
  PageTarget target = DomNode{
      .node_id = *input_id,
      .document_identifier = *DocumentIdentifierUserData::GetDocumentIdentifier(
          subframe->GetGlobalFrameToken())};
  std::vector<FileUploadSource> files;
  files.emplace_back(
      embedded_https_test_server().GetURL("example.com", kPayloadPath));
  std::unique_ptr<ToolRequest> action = std::make_unique<FileUploadToolRequest>(
      ActorSurfaceHandle::From(active_tab()->GetHandle()), std::move(target),
      std::move(files));

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(base::StrCat({"upload_payload.txt|",
                          base::NumberToString(sizeof(kPayloadContents) - 1),
                          "|", kPayloadContents}),
            EvalJs(subframe, "files_selected").ExtractString());
}

// If the document holding the target is gone by the time the files are staged,
// the action fails rather than handing the files to another document.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest,
                       FailsWhenTargetDocumentIsGone) {
  const GURL payload_url =
      embedded_https_test_server().GetURL("example.com", kPayloadPath);
  std::unique_ptr<ToolRequest> action =
      NavigateAndMakeUploadRequest({payload_url});
  ASSERT_TRUE(action);

  // Loading the page again replaces the document the target was taken from.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", kPageWithFileInput)));

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFrameWentAway);

  EXPECT_EQ(0, EvalJs(web_contents(),
                      "document.getElementById('fileinput').files.length")
                   .ExtractInt());
}

// A URL that cannot be fetched fails the action, and nothing is handed to the
// page.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest, FailedDownloadFails) {
  const GURL missing_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/does_not_exist.txt");
  std::unique_ptr<ToolRequest> action =
      NavigateAndMakeUploadRequest({missing_url});
  ASSERT_TRUE(action);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFileUploadDownloadFailed);

  EXPECT_EQ(0, EvalJs(web_contents(),
                      "document.getElementById('fileinput').files.length")
                   .ExtractInt());
}

// Files on a credentialed host are downloaded with the user's cookies,
// including SameSite ones, since the host only serves a file to its owner.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest,
                       SendsCookiesToCredentialedHost) {
  const GURL file_url = embedded_https_test_server().GetURL(
      kCredentialedHost, "/echoheader?Cookie");
  ASSERT_TRUE(content::SetCookie(web_contents()->GetBrowserContext(), file_url,
                                 "auth=1; Secure; SameSite=Lax"));
  std::unique_ptr<ToolRequest> action =
      NavigateAndMakeUploadRequest({file_url});
  ASSERT_TRUE(action);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ("auth=1", EvalJs(web_contents(), kFirstFileContents));
}

// Files on any other host are downloaded without the user's cookies, since the
// contents are handed to the page.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest,
                       OmitsCookiesForOtherHosts) {
  const GURL file_url =
      embedded_https_test_server().GetURL("bar.com", "/echoheader?Cookie");
  ASSERT_TRUE(content::SetCookie(web_contents()->GetBrowserContext(), file_url,
                                 "auth=1; Secure; SameSite=None"));
  std::unique_ptr<ToolRequest> action =
      NavigateAndMakeUploadRequest({file_url});
  ASSERT_TRUE(action);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ("None", EvalJs(web_contents(), kFirstFileContents));
}

// Redirects are not followed, so a download that is redirected fails, and
// nothing is handed to the page.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest,
                       RedirectedDownloadFails) {
  const GURL payload_url =
      embedded_https_test_server().GetURL("bar.com", kPayloadPath);
  const GURL file_url = embedded_https_test_server().GetURL(
      "example.com", base::StrCat({"/server-redirect?", payload_url.spec()}));
  std::unique_ptr<ToolRequest> action =
      NavigateAndMakeUploadRequest({file_url});
  ASSERT_TRUE(action);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFileUploadDownloadFailed);

  EXPECT_EQ(0, EvalJs(web_contents(),
                      "document.getElementById('fileinput').files.length")
                   .ExtractInt());
}

// Choosers opened after the staged files have been consumed are not
// intercepted a second time.
IN_PROC_BROWSER_TEST_F(ActorFileUploadToolBrowserTest,
                       StagingIsConsumedOnlyOnce) {
  const GURL payload_url =
      embedded_https_test_server().GetURL("example.com", kPayloadPath);
  std::unique_ptr<ToolRequest> action =
      NavigateAndMakeUploadRequest({payload_url});
  ASSERT_TRUE(action);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_FALSE(
      EvalJs(web_contents(), "files_selected").ExtractString().empty());

  scoped_refptr<content::FileSelectListener> listener =
      base::MakeRefCounted<NoopFileSelectListener>();
  blink::mojom::FileChooserParams params;
  params.mode = blink::mojom::FileChooserParams::Mode::kOpen;
  // Defaults to true, which the interceptor always declines; clear it so that
  // only the consumed staging can cause the request to be declined.
  params.need_local_path = false;
  EXPECT_FALSE(MaybeInterceptFileChooser(*main_frame(), listener, params));
  EXPECT_TRUE(listener);
}

}  // namespace
}  // namespace actor
