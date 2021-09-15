// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_BLOCKLIST_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_BLOCKLIST_H_

namespace ash {
namespace input_method {

class AssistiveSuggesterBlocklist {
 public:
  AssistiveSuggesterBlocklist() = default;
  virtual ~AssistiveSuggesterBlocklist() = default;

  // Are emoji suggestions allowed to be surfaced to the user?
  virtual bool IsEmojiSuggestionAllowed() = 0;

  // Are multi word suggestions allowed to be surfaced to the user?
  virtual bool IsMultiWordSuggestionAllowed() = 0;

  // Are personal info suggestions allowed to be surfaced to the user?
  virtual bool IsPersonalInfoSuggestionAllowed() = 0;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_BLOCKLIST_H_
