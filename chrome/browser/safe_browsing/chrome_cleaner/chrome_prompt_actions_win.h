// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_ACTIONS_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_ACTIONS_WIN_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace safe_browsing {

class ChromeCleanerScannerResults;

// Actions triggered from the ChromePrompt IPC interface.
//
// A ChromePromptActions object should be owned by the ChromePromptChannel that
// implements the IPC channel. It can be called to trigger an action for each
// message received.
class ChromePromptActions {
 public:
  // A callback to be called after showing the prompt, with the user's choice.
  using PromptUserReplyCallback = base::OnceCallback<void(
      chrome_cleaner::PromptUserResponse::PromptAcceptance)>;

  // A callback to show the prompt. The ChromeCleanerScannerResults contains
  // the items that were detected by the scanner, for display in the prompt.
  // The PromptUserCallback must call the PromptUserReplyCallback with the
  // user's choice.
  using PromptUserCallback =
      base::OnceCallback<void(ChromeCleanerScannerResults&&,
                              PromptUserReplyCallback)>;

  // Initializes the actions to use the |on_prompt_user| callback to display
  // the prompt when the PromptUser method is called.
  explicit ChromePromptActions(PromptUserCallback on_prompt_user);
  ~ChromePromptActions();

  // Calls the PromptUserCallback to show the prompt.  |reply_callback| will be
  // called with the user's choice.
  void PromptUser(
      const std::vector<base::FilePath>& files_to_delete,
      const absl::optional<std::vector<std::wstring>>& registry_keys,
      PromptUserReplyCallback reply_callback);

 private:
  ChromePromptActions(const ChromePromptActions& other) = delete;
  ChromePromptActions& operator=(const ChromePromptActions& other) = delete;

  // Callback that will be invoked when PromptUser is called to display the
  // prompt.
  PromptUserCallback on_prompt_user_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_ACTIONS_WIN_H_
