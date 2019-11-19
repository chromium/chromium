// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"
#include "chrome/browser/android/contextualsearch/resolved_search_term.h"
#include "chrome/browser/android/proto/client_discourse_context.pb.h"
#include "chrome/common/chrome_switches.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ListValue;

namespace {

const char kSomeSpecificBasePage[] = "http://some.specific.host.name.com/";
const char kDiscourseContextHeaderName[] = "X-Additional-Discourse-Context";

}  // namespace

class ContextualSearchDelegateTest : public testing::Test {
 public:
  ContextualSearchDelegateTest() {}
  ~ContextualSearchDelegateTest() override {}

 protected:
  void SetUp() override {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    template_url_service_.reset(CreateTemplateURLService());
    delegate_.reset(new ContextualSearchDelegate(
        test_shared_url_loader_factory_, template_url_service_.get(),
        base::Bind(
            &ContextualSearchDelegateTest::recordSearchTermResolutionResponse,
            base::Unretained(this)),
        base::Bind(
            &ContextualSearchDelegateTest::recordSampleSelectionAvailable,
            base::Unretained(this))));
  }

  void TearDown() override {
    is_invalid_ = true;
    response_code_ = -1;
    search_term_ = "invalid";
    mid_ = "";
    display_text_ = "unknown";
    context_language_ = "";
  }

  TemplateURLService* CreateTemplateURLService() {
    // Set a default search provider that supports Contextual Search.
    TemplateURLData data;
    data.SetURL("https://foobar.com/url?bar={searchTerms}");
    data.contextual_search_url = "https://foobar.com/_/contextualsearch?"
        "{google:contextualSearchVersion}{google:contextualSearchContextData}";
    TemplateURLService* template_url_service = new TemplateURLService(NULL, 0);
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
    return template_url_service;
  }

  void CreateDefaultSearchContextAndRequestSearchTerm() {
    base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
    CreateSearchContextAndRequestSearchTerm("Barack Obama", surrounding, 0, 6);
  }

  void CreateSearchContextAndRequestSearchTerm(
      const std::string& selected_text,
      const base::string16& surrounding_text,
      int start_offset,
      int end_offset) {
    test_context_ = new ContextualSearchContext(
        std::string(), GURL(kSomeSpecificBasePage), "utf-8");
    // ContextualSearchDelegate class takes ownership of the context.
    delegate_->SetContextForTesting(test_context_->GetWeakPtr());

    test_context_->SetSelectionSurroundings(start_offset, end_offset,
                                            surrounding_text);
    delegate_->ResolveSearchTermFromContext();
    ASSERT_TRUE(test_url_loader_factory_.GetPendingRequest(0));
  }

  // Allows using the vertical bar "|" as a quote character, which makes
  // test cases more readable versus the escaped double quote that is otherwise
  // needed for JSON literals.
  std::string escapeBarQuoted(std::string bar_quoted) {
    std::replace(bar_quoted.begin(), bar_quoted.end(), '|', '\"');
    return bar_quoted;
  }

  void CreateDefaultSearchWithAdditionalJsonData(
      const std::string additional_json_data) {
    CreateDefaultSearchContextAndRequestSearchTerm();
    std::string response =
        escapeBarQuoted("{|search_term|:|obama|" + additional_json_data + "}");
    SimulateResponseReturned(response);

    EXPECT_FALSE(is_invalid());
    EXPECT_EQ(200, response_code());
    EXPECT_EQ("obama", search_term());
  }

  //-------------------------------------------------------------------
  // Helper methods that call private ContextualSearchDelegate methods.
  // The ContextualSearchDelegate methods cannot be called directly
  // from tests, but can be called here because this is a friend class.
  //-------------------------------------------------------------------
  void CreateTestContext() {
    test_context_ = new ContextualSearchContext(
        std::string(), GURL(kSomeSpecificBasePage), "utf-8");
    delegate_->SetContextForTesting(test_context_->GetWeakPtr());
  }

  void DestroyTestContext() { delete test_context_; }

  // Call the OnTextSurroundingSelectionAvailable.
  // Cannot be in an actual test because OnTextSurroundingSelectionAvailable
  // is private.
  void CallOnTextSurroundingSelectionAvailable() {
    delegate_->OnTextSurroundingSelectionAvailable(base::string16(), 1, 2);
  }

  void CallResolveSearchTermFromContext() {
    delegate_->ResolveSearchTermFromContext();
  }

  void SetResponseStringAndSimulateResponse(const std::string& selected_text,
                                            const std::string& mentions_start,
                                            const std::string& mentions_end) {
    std::string response = std::string(
        ")]}'\n"
        "{\"mid\":\"/m/02mjmr\", \"search_term\":\"obama\","
        "\"info_text\":\"44th U.S. President\","
        "\"display_text\":\"Barack Obama\","
        "\"mentions\":[" + mentions_start + "," + mentions_end + "],"
        "\"selected_text\":\"" + selected_text + "\","
        "\"resolved_term\":\"barack obama\"}");
    SimulateResponseReturned(response);
  }

  void SimulateResponseReturned(const std::string& response) {
    auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request->request.url.spec(), response);
    base::RunLoop().RunUntilIdle();
  }

  void SetSurroundingContext(const base::string16& surrounding_text,
                             int start_offset,
                             int end_offset) {
    test_context_ = new ContextualSearchContext(
        std::string(), GURL(kSomeSpecificBasePage), "utf-8");
    test_context_->SetSelectionSurroundings(start_offset, end_offset,
                                            surrounding_text);
    // ContextualSearchDelegate class takes ownership of the context.
    delegate_->SetContextForTesting(test_context_->GetWeakPtr());
  }

  // Gets the Client Discourse Context proto from the request header.
  discourse_context::ClientDiscourseContext GetDiscourseContextFromRequest() {
    discourse_context::ClientDiscourseContext cdc;
    // Make sure we can get the actual raw headers from the url loader.
    auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
    net::HttpRequestHeaders request_headers = pending_request->request.headers;
    if (request_headers.HasHeader(kDiscourseContextHeaderName)) {
      std::string actual_header_value;
      request_headers.GetHeader(kDiscourseContextHeaderName,
                                &actual_header_value);

      // Unescape, since the server memoizer expects a web-safe encoding.
      std::string unescaped_header = actual_header_value;
      std::replace(unescaped_header.begin(), unescaped_header.end(), '-', '+');
      std::replace(unescaped_header.begin(), unescaped_header.end(), '_', '/');

      // Base64 decode the header.
      std::string decoded_header;
      if (base::Base64Decode(unescaped_header, &decoded_header)) {
        cdc.ParseFromString(decoded_header);
      }
    }
    return cdc;
  }

  // Gets the base-page URL from the request, or an empty string if not present.
  std::string getBasePageUrlFromRequest() {
    std::string result;
    discourse_context::ClientDiscourseContext cdc =
        GetDiscourseContextFromRequest();
    if (cdc.display_size() > 0) {
      const discourse_context::Display& first_display = cdc.display(0);
      result = first_display.uri();
    }
    return result;
  }

  // Accessors to response members
  bool is_invalid() { return is_invalid_; }
  int response_code() { return response_code_; }
  std::string search_term() { return search_term_; }
  std::string display_text() { return display_text_; }
  std::string alternate_term() { return alternate_term_; }
  std::string mid() { return mid_; }
  bool do_prevent_preload() { return prevent_preload_; }
  int start_adjust() { return start_adjust_; }
  int end_adjust() { return end_adjust_; }
  std::string context_language() { return context_language_; }
  std::string thumbnail_url() { return thumbnail_url_; }
  std::string caption() { return caption_; }
  std::string quick_action_uri() { return quick_action_uri_; }
  QuickActionCategory quick_action_category() { return quick_action_category_; }
  int64_t logged_event_id() { return logged_event_id_; }
  std::string search_url_full() { return search_url_full_; }
  std::string search_url_preload() { return search_url_preload_; }
  int coca_card_tag() { return coca_card_tag_; }

  // The delegate under test.
  std::unique_ptr<ContextualSearchDelegate> delegate_;

  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  void recordSearchTermResolutionResponse(
      const ResolvedSearchTerm& resolved_search_term) {
    is_invalid_ = resolved_search_term.is_invalid;
    response_code_ = resolved_search_term.response_code;
    search_term_ = resolved_search_term.search_term;
    display_text_ = resolved_search_term.display_text;
    alternate_term_ = resolved_search_term.alternate_term;
    mid_ = resolved_search_term.mid;
    prevent_preload_ = resolved_search_term.prevent_preload;
    start_adjust_ = resolved_search_term.selection_start_adjust;
    end_adjust_ = resolved_search_term.selection_end_adjust;
    context_language_ = resolved_search_term.context_language;
    thumbnail_url_ = resolved_search_term.thumbnail_url;
    caption_ = resolved_search_term.caption;
    quick_action_uri_ = resolved_search_term.quick_action_uri;
    quick_action_category_ = resolved_search_term.quick_action_category;
    logged_event_id_ = resolved_search_term.logged_event_id;
    search_url_full_ = resolved_search_term.search_url_full;
    search_url_preload_ = resolved_search_term.search_url_preload;
    coca_card_tag_ = resolved_search_term.coca_card_tag;
  }

  void recordSampleSelectionAvailable(const std::string& encoding,
                                      const base::string16& surrounding_text,
                                      size_t start_offset,
                                      size_t end_offset) {
    // unused.
  }

  // Local response members
  bool is_invalid_;
  int response_code_;
  std::string search_term_;
  std::string display_text_;
  std::string alternate_term_;
  std::string mid_;
  bool prevent_preload_;
  int start_adjust_;
  int end_adjust_;
  std::string context_language_;
  std::string thumbnail_url_;
  std::string caption_;
  std::string quick_action_uri_;
  QuickActionCategory quick_action_category_;
  int64_t logged_event_id_;
  std::string search_url_full_;
  std::string search_url_preload_;
  int coca_card_tag_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<TemplateURLService> template_url_service_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  // Will be owned by the delegate.
  ContextualSearchContext* test_context_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchDelegateTest);
};

TEST_F(ContextualSearchDelegateTest, NormalFetchWithXssiEscape) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  std::string response(
      ")]}'\n"
      "{\"mid\":\"/m/02mjmr\", \"search_term\":\"obama\","
      "\"info_text\":\"44th U.S. President\","
      "\"display_text\":\"Barack Obama\", \"mentions\":[0,15],"
      "\"selected_text\":\"obama\", \"resolved_term\":\"barack obama\"}");
  SimulateResponseReturned(response);

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("Barack Obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_FALSE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, NormalFetchWithoutXssiEscape) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  std::string response(
      "{\"mid\":\"/m/02mjmr\", \"search_term\":\"obama\","
      "\"info_text\":\"44th U.S. President\","
      "\"display_text\":\"Barack Obama\", \"mentions\":[0,15],"
      "\"selected_text\":\"obama\", \"resolved_term\":\"barack obama\"}");
  SimulateResponseReturned(response);

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("Barack Obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_FALSE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, ResponseWithNoDisplayText) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  std::string response(
      "{\"mid\":\"/m/02mjmr\",\"search_term\":\"obama\","
      "\"mentions\":[0,15]}");
  SimulateResponseReturned(response);

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_FALSE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, ResponseWithPreventPreload) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  std::string response(
      "{\"mid\":\"/m/02mjmr\",\"search_term\":\"obama\","
      "\"mentions\":[0,15],\"prevent_preload\":\"1\"}");
  SimulateResponseReturned(response);

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_TRUE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, NonJsonResponse) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  std::string response("Non-JSON Response");
  SimulateResponseReturned(response);

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("", search_term());
  EXPECT_EQ("", display_text());
  EXPECT_EQ("", mid());
  EXPECT_FALSE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, InvalidResponse) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url, network::URLLoaderCompletionStatus(net::OK),
      network::mojom::URLResponseHead::New(), std::string());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(do_prevent_preload());
  EXPECT_TRUE(is_invalid());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionToEnd) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Barack";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 0, 6);
  SetResponseStringAndSimulateResponse(selected_text, "0", "12");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(6, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionToStart) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Obama";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 7, 12);
  SetResponseStringAndSimulateResponse(selected_text, "0", "12");

  EXPECT_EQ(-7, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionBothDirections) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 7, 9);
  SetResponseStringAndSimulateResponse(selected_text, "0", "12");

  EXPECT_EQ(-7, start_adjust());
  EXPECT_EQ(3, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionInvalidRange) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 7, 9);
  SetResponseStringAndSimulateResponse(selected_text, "0", "200");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionInvalidDistantStart) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding,
                                          0xffffffff, 0xffffffff - 2);
  SetResponseStringAndSimulateResponse(selected_text, "0", "12");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionInvalidNoOverlap) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 0, 12);
  SetResponseStringAndSimulateResponse(selected_text, "12", "14");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionInvalidDistantEndAndRange) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding,
                                          0xffffffff, 0xffffffff - 2);
  SetResponseStringAndSimulateResponse(selected_text, "0", "268435455");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionLargeNumbers) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding,
                                          268435450, 268435455);
  SetResponseStringAndSimulateResponse(selected_text, "268435440", "268435455");

  EXPECT_EQ(-10, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ContractSelectionValid) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Barack Obama just";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 0, 17);
  SetResponseStringAndSimulateResponse(selected_text, "0", "12");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(-5, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ContractSelectionInvalid) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Barack Obama just";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 0, 17);
  SetResponseStringAndSimulateResponse(selected_text, "5", "5");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExtractMentionsStartEnd) {
  ListValue mentions_list;
  mentions_list.AppendInteger(1);
  mentions_list.AppendInteger(2);
  int start = 0;
  int end = 0;
  delegate_->ExtractMentionsStartEnd(mentions_list, &start, &end);
  EXPECT_EQ(1, start);
  EXPECT_EQ(2, end);
}

TEST_F(ContextualSearchDelegateTest, SampleSurroundingText) {
  base::string16 sample = base::ASCIIToUTF16("this is Barack Obama in office.");
  int limit_each_side = 3;
  size_t start = 8;
  size_t end = 20;
  base::string16 result =
      delegate_->SampleSurroundingText(sample, limit_each_side, &start, &end);
  EXPECT_EQ(static_cast<size_t>(3), start);
  EXPECT_EQ(static_cast<size_t>(15), end);
  EXPECT_EQ(base::ASCIIToUTF16("is Barack Obama in"), result);
}

TEST_F(ContextualSearchDelegateTest, SampleSurroundingTextNegativeLimit) {
  base::string16 sample = base::ASCIIToUTF16("this is Barack Obama in office.");
  int limit_each_side = -2;
  size_t start = 8;
  size_t end = 20;
  base::string16 result =
      delegate_->SampleSurroundingText(sample, limit_each_side, &start, &end);
  EXPECT_EQ(static_cast<size_t>(0), start);
  EXPECT_EQ(static_cast<size_t>(12), end);
  EXPECT_EQ(base::ASCIIToUTF16("Barack Obama"), result);
}

TEST_F(ContextualSearchDelegateTest, SampleSurroundingTextSameStartEnd) {
  base::string16 sample = base::ASCIIToUTF16("this is Barack Obama in office.");
  int limit_each_side = 3;
  size_t start = 11;
  size_t end = 11;
  base::string16 result =
      delegate_->SampleSurroundingText(sample, limit_each_side, &start, &end);
  VLOG(0) << "start " << start;
  VLOG(0) << "end " << end;
  VLOG(0) << "result " << result;
  EXPECT_EQ(static_cast<size_t>(3), start);
  EXPECT_EQ(static_cast<size_t>(3), end);
  EXPECT_EQ(base::ASCIIToUTF16("Barack"), result);
}

TEST_F(ContextualSearchDelegateTest, DecodeSearchTermFromJsonResponse) {
  std::string json_with_escape =
      ")]}'\n"
      "{\"mid\":\"/m/02mjmr\", \"search_term\":\"obama\","
      "\"info_text\":\"44th U.S. President\","
      "\"display_text\":\"Barack Obama\", \"mentions\":[0,15],"
      "\"selected_text\":\"obama\", \"resolved_term\":\"barack obama\","
      "\"logged_event_id\":\"1234567890123456789\","
      "\"search_url_full\":\"https://www.google.com/"
      "search?q=define+obscure&ctxs=2\","
      "\"search_url_preload\":\"https://www.google.com/"
      "search?q=define+obscure&ctxs=2&pf=c&sns=1\","
      "\"card_tag\":12"
      "}";
  std::string search_term;
  std::string display_text;
  std::string alternate_term;
  std::string mid;
  std::string prevent_preload;
  int mention_start;
  int mention_end;
  std::string context_language;
  std::string thumbnail_url;
  std::string caption;
  std::string quick_action_uri;
  int64_t logged_event_id;
  QuickActionCategory quick_action_category = QUICK_ACTION_CATEGORY_NONE;
  std::string search_url_full;
  std::string search_url_preload;
  int coca_card_tag;

  delegate_->DecodeSearchTermFromJsonResponse(
      json_with_escape, &search_term, &display_text, &alternate_term, &mid,
      &prevent_preload, &mention_start, &mention_end, &context_language,
      &thumbnail_url, &caption, &quick_action_uri, &quick_action_category,
      &logged_event_id, &search_url_full, &search_url_preload, &coca_card_tag);

  EXPECT_EQ("obama", search_term);
  EXPECT_EQ("Barack Obama", display_text);
  EXPECT_EQ("barack obama", alternate_term);
  EXPECT_EQ("/m/02mjmr", mid);
  EXPECT_EQ("", prevent_preload);
  EXPECT_EQ("", context_language);
  EXPECT_EQ("", thumbnail_url);
  EXPECT_EQ("", caption);
  EXPECT_EQ("", quick_action_uri);
  EXPECT_EQ(QUICK_ACTION_CATEGORY_NONE, quick_action_category);
  EXPECT_EQ(1234567890123456789, logged_event_id);
  EXPECT_EQ("https://www.google.com/search?q=define+obscure&ctxs=2",
            search_url_full);
  EXPECT_EQ("https://www.google.com/search?q=define+obscure&ctxs=2&pf=c&sns=1",
            search_url_preload);
  EXPECT_EQ(12, coca_card_tag);
}

TEST_F(ContextualSearchDelegateTest, ResponseWithLanguage) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  std::string response(
      "{\"mid\":\"/m/02mjmr\",\"search_term\":\"obama\","
      "\"mentions\":[0,15],\"prevent_preload\":\"1\", "
      "\"lang\":\"de\"}");
  SimulateResponseReturned(response);

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_TRUE(do_prevent_preload());
  EXPECT_EQ("de", context_language());
}

TEST_F(ContextualSearchDelegateTest, HeaderContainsBasePageUrl) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  EXPECT_EQ(kSomeSpecificBasePage, getBasePageUrlFromRequest());
}

// Missing all Contextual Cards data.
TEST_F(ContextualSearchDelegateTest, ContextualCardsResponseWithNoData) {
  CreateDefaultSearchWithAdditionalJsonData("");
  EXPECT_EQ("", caption());
  EXPECT_EQ("", thumbnail_url());
}

// Test just the root level caption.
TEST_F(ContextualSearchDelegateTest, ContextualCardsResponseWithCaption) {
  CreateDefaultSearchWithAdditionalJsonData(", |caption|:|aCaption|");
  EXPECT_EQ("aCaption", caption());
  EXPECT_EQ("", thumbnail_url());
}

// Test just the root level thumbnail.
TEST_F(ContextualSearchDelegateTest, ContextualCardsResponseWithThumbnail) {
  CreateDefaultSearchWithAdditionalJsonData(
      ", |thumbnail|:|https://t0.gstatic.com/images?q=tbn:ANd9|");
  EXPECT_EQ("", caption());
  EXPECT_EQ("https://t0.gstatic.com/images?q=tbn:ANd9", thumbnail_url());
}

// Test that we can destroy the context while resolving without a crash.
// Test is flaky: https://crbug.com/890427
TEST_F(ContextualSearchDelegateTest, DISABLED_DestroyContextDuringResolve) {
  CreateTestContext();
  CallResolveSearchTermFromContext();
  DestroyTestContext();

  std::string response("Any response as it does not matter here.");
  SimulateResponseReturned(response);

  EXPECT_TRUE(is_invalid());
}

// Test that we can destroy the context while gathering surrounding text.
TEST_F(ContextualSearchDelegateTest, DestroyContextDuringGatherSurroundings) {
  CreateTestContext();
  DestroyTestContext();
  CallOnTextSurroundingSelectionAvailable();
}

TEST_F(ContextualSearchDelegateTest, ResponseWithCocaCardTag) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  std::string response(
      "{\"search_term\":\"obscure\","
      "\"card_tag\":11}");
  SimulateResponseReturned(response);
  EXPECT_EQ("obscure", search_term());
  EXPECT_EQ(11, coca_card_tag());
}

TEST_F(ContextualSearchDelegateTest, ResponseWithoutCocaCardTag) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  std::string response("{\"search_term\":\"obscure\"}");
  SimulateResponseReturned(response);
  EXPECT_EQ("obscure", search_term());
  EXPECT_EQ(0, coca_card_tag());
}

TEST_F(ContextualSearchDelegateTest, ResponseWithStringCocaCardTag) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  std::string response(
      "{\"search_term\":\"obscure\","
      "\"card_tag\":\"11\"}");
  SimulateResponseReturned(response);
  EXPECT_EQ("obscure", search_term());
  EXPECT_EQ(0, coca_card_tag());
}
