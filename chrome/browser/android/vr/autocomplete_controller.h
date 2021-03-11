// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_AUTOCOMPLETE_CONTROLLER_H_
#define CHROME_BROWSER_ANDROID_VR_AUTOCOMPLETE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "url/gurl.h"

class AutocompleteController;
class ChromeAutocompleteProviderClient;
class Profile;

namespace vr {

class AutocompleteController : public ::AutocompleteController::Observer {
 public:
  using SuggestionCallback =
      base::RepeatingCallback<void(std::vector<OmniboxSuggestion>)>;

  explicit AutocompleteController(SuggestionCallback callback);
  AutocompleteController();
  ~AutocompleteController() override;

  void Start(const AutocompleteRequest& request);
  void Stop();

  // If |input| can be classified as URL, this function returns a GURL
  // representation of that URL and true. Otherwise, it returns a GURL which
  // navigates to the default search engine with |input| as query and false.
  // This function runs independently of any currently-running autocomplete
  // session.
  std::tuple<GURL, bool> GetUrlFromVoiceInput(const std::u16string& input);

 private:
  // ::AutocompleteController::Observer:
  void OnResultChanged(::AutocompleteController* controller,
                       bool default_match_changed) override;

  Profile* profile_;
  ChromeAutocompleteProviderClient* client_;
  std::unique_ptr<::AutocompleteController> autocomplete_controller_;
  SuggestionCallback suggestion_callback_;
  AutocompleteRequest last_request_;

  // This is used to throttle the rate at which new suggestions are presented to
  // the user. For example, if a suggestion comes in on frame 1 and frame 2, we
  // will wait for a period of time after the receipt of each suggestion and
  // batch incoming suggestions that arrive before that period of time has been
  // exceeded.
  base::CancelableOnceClosure suggestions_timeout_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteController);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_AUTOCOMPLETE_CONTROLLER_H_
