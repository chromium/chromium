// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_input_denylist.h"

#include "chrome/browser/ash/input_method/field_trial.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace input_method {

std::string Empty() {
  return "";
}

std::string InvalidJson() {
  return "[:]";
}

std::string ExampleDenylist() {
  return "[\"xyz\", \"uk\", \"subdomain.blah\"]";
}

using Contains = testing::TestWithParam<std::string>;

INSTANTIATE_TEST_SUITE_P(
    AssistiveInputDenylistTest,
    Contains,
    testing::Values(
        "https://amazon.com",
        "https://b.corp.google.com",
        "https://buganizer.corp.google.com",
        "https://cider.corp.google.com",
        "https://classroom.google.com",
        "https://desmos.com",
        "https://docs.google.com",
        "https://facebook.com",
        "https://instagram.com",
        "https://outlook.live.com",
        "https://outlook.office.com",
        "https://quizlet.com",
        "https://reddit.com",
        "https://teams.microsoft.com",
        "https://twitter.com",
        "https://whatsapp.com",
        "https://www.youtube.com",
        "https://b.corp.google.com/134",
        "https://docs.google.com/document/d/documentId/edit",
        "https://amazon.com.au",
        "https://amazon.com.au/gp/new-releases",
        "http://smile.amazon.com",
        "http://www.abc.smile.amazon.com.au/abc+com+au/some/other/text"));

TEST_P(Contains, Url) {
  EXPECT_TRUE(AssistiveInputDenylist(
                  DenylistAdditions{.autocorrect_denylist_json = Empty(),
                                    .multi_word_denylist_json = Empty()})
                  .Contains(GURL(GetParam())));
}

TEST_P(Contains, UrlWhenInvalidJsonPassedAsAutocorrectDenylist) {
  EXPECT_TRUE(AssistiveInputDenylist(
                  DenylistAdditions{.autocorrect_denylist_json = InvalidJson(),
                                    .multi_word_denylist_json = Empty()})
                  .Contains(GURL(GetParam())));
}

TEST_P(Contains, UrlWhenInvalidJsonPassedAsMultiWordDenylist) {
  EXPECT_TRUE(AssistiveInputDenylist(
                  DenylistAdditions{.autocorrect_denylist_json = Empty(),
                                    .multi_word_denylist_json = InvalidJson()})
                  .Contains(GURL(GetParam())));
}

TEST_P(Contains, UrlWhenAutocorrectDynamicDenylistGiven) {
  EXPECT_TRUE(
      AssistiveInputDenylist(
          DenylistAdditions{.autocorrect_denylist_json = ExampleDenylist(),
                            .multi_word_denylist_json = Empty()})
          .Contains(GURL(GetParam())));
}

TEST_P(Contains, UrlWhenMultiWordDynamicDenylistGiven) {
  EXPECT_TRUE(
      AssistiveInputDenylist(
          DenylistAdditions{.autocorrect_denylist_json = Empty(),
                            .multi_word_denylist_json = ExampleDenylist()})
          .Contains(GURL(GetParam())));
}

using DoesNotContain = testing::TestWithParam<std::string>;

INSTANTIATE_TEST_SUITE_P(
    AssistiveInputDenylistTest,
    DoesNotContain,
    testing::Values("",
                    "http://",
                    "http://abc.com",
                    "http://abc.com/amazon+com",
                    "http://amazon",
                    "http://amazon/test",
                    "http://amazon.domain.com",
                    "http://smile.amazon.foo.com",
                    "http://my.own.quizlet.uniquie.co.uk/testing",
                    "http://sites.google.com/view/e14s-test",
                    "http://amazon/com/test",
                    "http://not-amazon.com/test",
                    "http://.com/test"));

TEST_P(DoesNotContain, Url) {
  EXPECT_FALSE(AssistiveInputDenylist(
                   DenylistAdditions{.autocorrect_denylist_json = Empty(),
                                     .multi_word_denylist_json = Empty()})
                   .Contains(GURL(GetParam())));
}

TEST_P(DoesNotContain, UrlWhenInvalidJsonPassedAsAutocorrectDenylist) {
  EXPECT_FALSE(AssistiveInputDenylist(
                   DenylistAdditions{.autocorrect_denylist_json = InvalidJson(),
                                     .multi_word_denylist_json = Empty()})
                   .Contains(GURL(GetParam())));
}

TEST_P(DoesNotContain, UrlWhenInvalidJsonPassedAsMultiWordDenylist) {
  EXPECT_FALSE(AssistiveInputDenylist(
                   DenylistAdditions{.autocorrect_denylist_json = Empty(),
                                     .multi_word_denylist_json = InvalidJson()})
                   .Contains(GURL(GetParam())));
}

TEST_P(DoesNotContain, UrlWhenAutocorrectDynamicDenylistGiven) {
  EXPECT_FALSE(
      AssistiveInputDenylist(
          DenylistAdditions{.autocorrect_denylist_json = ExampleDenylist(),
                            .multi_word_denylist_json = Empty()})
          .Contains(GURL(GetParam())));
}

TEST_P(DoesNotContain, UrlWhenMultiWordDynamicDenylistGiven) {
  EXPECT_FALSE(
      AssistiveInputDenylist(
          DenylistAdditions{.autocorrect_denylist_json = Empty(),
                            .multi_word_denylist_json = ExampleDenylist()})
          .Contains(GURL(GetParam())));
}

struct UrlExample {
  std::string url;
  bool found_in_list;
};

using UsesDynamicDenylist = testing::TestWithParam<UrlExample>;

INSTANTIATE_TEST_SUITE_P(
    AssistiveInputDenylistTest,
    UsesDynamicDenylist,
    testing::Values(
        // Disabled examples
        UrlExample{"http://xyz.com", /*found_in_list=*/true},
        UrlExample{"https://xyz.com", /*found_in_list=*/true},
        UrlExample{"https://www.xyz.com", /*found_in_list=*/true},
        UrlExample{"https://xyz.com/with-path", /*found_in_list=*/true},
        UrlExample{"https://xyz.com/with-path?and=params",
                   /*found_in_list=*/true},
        UrlExample{"https://uk.com", /*found_in_list=*/true},
        UrlExample{"https://subdomain.blah.com", /*found_in_list=*/true},
        UrlExample{"https://subdomain.blah.com/with-path",
                   /*found_in_list=*/true},
        // Enabled examples
        UrlExample{"https://www.something.co.uk", /*found_in_list=*/false},
        UrlExample{"https://something.co.uk/with-path",
                   /*found_in_list=*/false},
        UrlExample{"https://blah.com", /*found_in_list=*/false},
        UrlExample{"https://something.blah.com", /*found_in_list=*/false},
        UrlExample{"https://something.blah.com/with-path",
                   /*found_in_list=*/false}));

TEST_P(UsesDynamicDenylist, ForAutocorrect) {
  const UrlExample& example = GetParam();

  EXPECT_EQ(
      AssistiveInputDenylist(
          DenylistAdditions{.autocorrect_denylist_json = ExampleDenylist(),
                            .multi_word_denylist_json = Empty()})
          .Contains(GURL(example.url)),
      example.found_in_list);
}

TEST_P(UsesDynamicDenylist, ForMultiWord) {
  const UrlExample& example = GetParam();

  EXPECT_EQ(
      AssistiveInputDenylist(
          DenylistAdditions{.autocorrect_denylist_json = Empty(),
                            .multi_word_denylist_json = ExampleDenylist()})
          .Contains(GURL(example.url)),
      example.found_in_list);
}

}  // namespace input_method
}  // namespace ash
