// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_DELEGATE_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"
#include "chrome/browser/android/contextualsearch/resolved_search_term.h"

namespace content {
class WebContents;
}

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class Profile;
class TemplateURLService;
class ContextualSearchFieldTrial;

// Handles tasks for the ContextualSearchManager in a separable and testable
// way, without the complication of being connected to a Java object.
class ContextualSearchDelegate
    : public base::SupportsWeakPtr<ContextualSearchDelegate> {
 public:
  // Provides the Resolved Search Term, called when the Resolve Request returns.
  typedef base::Callback<void(const ResolvedSearchTerm&)>
      SearchTermResolutionCallback;
  // Provides text surrounding the selection to Java.
  typedef base::Callback<
      void(const std::string&, const base::string16&, size_t, size_t)>
      SurroundingTextCallback;

  // Constructs a delegate that will always call back to the given callbacks
  // when search term resolution or surrounding text responses are available.
  ContextualSearchDelegate(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TemplateURLService* template_url_service,
      const SearchTermResolutionCallback& search_term_callback,
      const SurroundingTextCallback& surrounding_callback);
  virtual ~ContextualSearchDelegate();

  // Gathers surrounding text and saves it locally in the given context.
  void GatherAndSaveSurroundingText(
      base::WeakPtr<ContextualSearchContext> contextual_search_context,
      content::WebContents* web_contents);

  // If the caller chooses not to call |GatherAndSaveSurroundingText| then they
  // must call this method to set the active context before calling
  // |StartSearchTermResolutionRequest|.
  void SetActiveContext(
      base::WeakPtr<ContextualSearchContext> contextual_search_context);

  // Starts an asynchronous search term resolution request.
  // The given context includes some content from a web page and must be able
  // to resolve.
  // When the response is available the callback specified in the constructor
  // is run.
  void StartSearchTermResolutionRequest(
      base::WeakPtr<ContextualSearchContext> contextual_search_context,
      content::WebContents* web_contents);

  // Gets the target language for translation purposes for this user.
  // TODO(donnd): remove these language accessors once the transition to the
  // Chrome Language Model is complete.
  std::string GetTargetLanguage();

  // Returns the accept languages preference string.
  std::string GetAcceptLanguages();

 private:
  // Friend our test which allows our private methods to be used in helper
  // functions.  FRIEND_TEST_ALL_PREFIXES just friends individual prefixes.
  // Needed for |ResolveSearchTermFromContext|.
  friend class ContextualSearchDelegateTest;
  // TODO(donnd): consider removing the following since the above covers this.
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SurroundingTextHighMaximum);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SurroundingTextLowMaximum);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SurroundingTextNoBeforeText);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SurroundingTextNoAfterText);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           ExtractMentionsStartEnd);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest, SampleSurroundingText);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SampleSurroundingTextNegativeLimit);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SampleSurroundingTextSameStartEnd);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           DecodeSearchTermFromJsonResponse);

  void OnUrlLoadComplete(std::unique_ptr<std::string> response_body);

  // Resolves the search term specified by the current context.
  // Only needed for tests.  TODO(donnd): make private and friend?
  void ResolveSearchTermFromContext();

  // Builds and returns the search term resolution request URL.
  // |context| is used to help build the query.
  std::string BuildRequestUrl(ContextualSearchContext* context);

  // Uses the TemplateURL service to construct a search term resolution URL from
  // the given parameters.
  std::string GetSearchTermResolutionUrlString(
      const std::string& selected_text,
      const std::string& base_page_url,
      const bool may_send_base_page_url);

  void OnTextSurroundingSelectionAvailable(
      const base::string16& surrounding_text,
      uint32_t start_offset,
      uint32_t end_offset);

  // Populates and returns the discourse context.
  std::string GetDiscourseContext(const ContextualSearchContext& context);

  // Checks if we can send the URL for this user. Several conditions are checked
  // to make sure it's OK to send the URL.  These fall into two categories:
  // 1) check if it's allowed by our policy, and 2) ensure that the user is
  // already sending their URL browsing activity to Google.
  bool CanSendPageURL(const GURL& current_page_url,
                      Profile* profile,
                      TemplateURLService* template_url_service);

  // Builds a Resolved Search Term by decoding the given JSON string.
  std::unique_ptr<ResolvedSearchTerm> GetResolvedSearchTermFromJson(
      int response_code,
      const std::string& json_string);

  // Decodes the given json response string and extracts parameters.
  void DecodeSearchTermFromJsonResponse(
      const std::string& response,
      std::string* search_term,
      std::string* display_text,
      std::string* alternate_term,
      std::string* mid,
      std::string* prevent_preload,
      int* mention_start,
      int* mention_end,
      std::string* context_language,
      std::string* thumbnail_url,
      std::string* caption,
      std::string* quick_action_uri,
      QuickActionCategory* quick_action_category,
      int64_t* logged_event_id,
      std::string* search_url_full,
      std::string* search_url_preload,
      int* coca_card_tag);

  // Extracts the start and end location from a mentions list, and sets the
  // integers referenced by |startResult| and |endResult|.
  void ExtractMentionsStartEnd(const base::ListValue& mentions_list,
                               int* startResult,
                               int* endResult);

  // Generates a subset of the given surrounding_text string, for usage from
  // Java.
  // |surrounding_text| the entire text context that contains the selection.
  // |padding_each_side| the number of characters of padding desired on each
  // side of the selection (negative values treated as 0).
  // |start| the start offset of the selection, updated to reflect the new
  // position
  // of the selection in the function result.
  // |end| the end offset of the selection, updated to reflect the new position
  // of the selection in the function result.
  // |return| the trimmed surrounding text with selection at the
  // updated start/end offsets.
  base::string16 SampleSurroundingText(const base::string16& surrounding_text,
                                       int padding_each_side,
                                       size_t* start,
                                       size_t* end);

  // For testing.
  void SetContextForTesting(
      const base::WeakPtr<ContextualSearchContext>& context) {
    context_ = context;
  }

  // The current request in progress, or NULL.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Holds the URL loader factory.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Holds the TemplateURLService. Not owned.
  TemplateURLService* template_url_service_;

  // The field trial helper instance, always set up by the constructor.
  std::unique_ptr<ContextualSearchFieldTrial> field_trial_;

  // The callback for notifications of completed URL fetches.
  SearchTermResolutionCallback search_term_callback_;

  // The callback for notifications of surrounding text being available.
  SurroundingTextCallback surrounding_text_callback_;

  // Used to hold the context until an upcoming search term request is started.
  // Owned by the Java ContextualSearchContext.
  base::WeakPtr<ContextualSearchContext> context_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_DELEGATE_H_
