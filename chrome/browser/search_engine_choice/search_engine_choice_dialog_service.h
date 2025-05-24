// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_SERVICE_H_
#define CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_SERVICE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/template_url_data.h"

class Browser;
class TemplateURLService;

namespace search_engines {

class ChoiceScreenData;
enum class SearchEngineChoiceScreenConditions;

// Profile specific data related to the search engine choice.
// `timestamp` is the search engine choice timestamp that's saved in the
// `kDefaultSearchProviderChoiceScreenCompletionTimestamp` pref.
// `chrome_version` is the Chrome version when the user made the choice.
// `default_search_engine` is the profile's default search engine.
struct ChoiceData {
  int64_t timestamp = 0;
  std::string chrome_version;
  TemplateURLData default_search_engine;
};

}  // namespace search_engines

// Service handling the Search Engine Choice dialog.
class SearchEngineChoiceDialogService : public KeyedService {
 public:
  // Specifies the view in which the choice screen UI is rendered.
  enum class EntryPoint {
    kProfileCreation = 0,
    kFirstRunExperience,
    kDialog,
  };

  SearchEngineChoiceDialogService(
      Profile& profile,
      search_engines::SearchEngineChoiceService& search_engine_choice_service,
      TemplateURLService& template_url_service);
  ~SearchEngineChoiceDialogService() override;

  // Informs the service that a Search Engine Choice dialog is being opened
  // for `browser`, providing a `close_dialog_callback` so that the dialog
  // can be closed when the choice is made, even from another browser
  // associated with this profile.
  //
  // Returns whether the caller should proceed with adding a dialog to
  // `browser`. When `false` is returned, the dialog will not be registered and
  // the caller should not proceed with showing the dialog.
  virtual bool RegisterDialog(Browser& browser,
                              base::OnceClosure close_dialog_callback);

  // This function is called when the user makes a search engine choice. It
  // closes the dialogs that are open on other browser windows that
  // have the same profile as the one on which the choice was made and sets the
  // corresponding preferences.
  // `prepopulate_id` is the `prepopulate_id` of the search engine found in
  // `components/search_engines/template_url_data.h`. It will always be > 0.
  // `save_guest_mode_selection` will save the guest mode selection so that the
  // users are not prompted at every guest session launch.
  // `entry_point` is the view in which the UI is rendered. Virtual to be able
  // to mock in tests.
  virtual void NotifyChoiceMade(int prepopulate_id,
                                bool save_guest_mode_selection,
                                EntryPoint entry_point);

  // Informs the service that the learn more link was clicked. This is used to
  // record histograms. `entry_point` is the view in which the UI is rendered.
  void NotifyLearnMoreLinkClicked(EntryPoint entry_point);

  // Informs the service that the "More" button was clicked. This is used to
  // record histograms. `entry_point` is the view in which the UI is rendered.
  void NotifyMoreButtonClicked(EntryPoint entry_point);

  // Returns the eligibility status for newly triggering a choice screen dialog.
  //
  // If calling this ahead of requesting to show the dialog, prefer to call
  // `SearchEngineChoiceTabHelper::MaybeShowDialog()` instead. It will check a
  // few more things and ensure we log the event correctly.
  search_engines::SearchEngineChoiceScreenConditions ComputeDialogConditions(
      Browser& browser) const;

  // Returns whether the dialog should be displayed over the passed URL.
  bool IsUrlSuitableForDialog(GURL url);

  // Returns whether a Search Engine Choice dialog is currently open or not for
  // `browser`.
  bool IsShowingDialog(Browser& browser) const;

  // Returns whether the Search Engine Choice dialog is either shown or
  // pending to be shown.
  bool HasPendingDialog(Browser& browser) const;

  // Checks whether we need to display the Privacy Sandbox dialog
  // in context of the Search Engine Choice.
  // The Privacy Sandbox dialog should be delayed to the next Chrome run if the
  // Search Engine Choice dialog will be or has been displayed in the current
  // run.
  // The Privacy Sandbox dialog should be displayed directly when the
  // browser gets launched in the case where the search engine choice was made
  // in the FRE.
  bool CanSuppressPrivacySandboxPromo() const;

  // Returns the list of search engines.
  // The search engine details returned by this function will be the canonical
  // ones and will not be affected by changes in search engine details from the
  // settings page.
  // Virtual to be able to mock in tests.
  virtual TemplateURL::TemplateURLVector GetSearchEngines();

  // Disables the display of the Search Engine Choice dialog for testing. When
  // `dialog_disabled` is true, `CanShowDialog` will return false.
  // NOTE: This is set to true in InProcessBrowserTest::SetUp, disabling the
  // dialog for those tests. If you set this outside of that context, you should
  // ensure it is reset at the end of your test.
  static void SetDialogDisabledForTests(bool dialog_disabled);

  // Returns a copy of the `ChoiceData` specific to `profile`.
  static search_engines::ChoiceData GetChoiceDataFromProfile(Profile& profile);

  // Updates `profile` with the values from `choice_data`.
  static void UpdateProfileFromChoiceData(
      Profile& profile,
      const search_engines::ChoiceData& choice_data);

 private:
  friend class SearchEngineChoiceDialogServiceFactory;

  // Keeps track of browsers that are known to be showing the choice dialog
  // to make sure that we can keep new browsers blocked until a choice is made
  // and unblock all of them when it is done.
  class BrowserRegistry : public BrowserListObserver {
   public:
    explicit BrowserRegistry(SearchEngineChoiceDialogService& service);
    ~BrowserRegistry() override;

    // Returns whether the provided browser requested to open a dialog. Note
    // that the dialog might have since been closed.
    bool IsRegistered(Browser& browser) const;

    // Returns whether a the provided browser is currently marked as having an
    // open dialog.
    bool HasOpenDialog(Browser& browser) const;

    // Registers that the browser wants to show a dialog. Returns whether the
    // registration is accepted. If `false` is returned, the browser should
    // abandon showing the dialog.
    bool RegisterBrowser(Browser& browser,
                         base::OnceClosure close_dialog_callback);
    void CloseAllDialogs();

    // BrowserListObserver implementation:
    void OnBrowserRemoved(Browser* browser) override;

   private:
    raw_ref<SearchEngineChoiceDialogService>
        search_engine_choice_dialog_service_;

    // A map of Browser windows which have registered with the service to show a
    // Search Engine Choice dialog, associated with a callback that will close
    // the browser's dialog.
    // The callback might be null, indicating that the browser showed a dialog
    // in the past, but that is has since been closed.
    base::flat_map<raw_ref<Browser>, base::OnceClosure> registered_browsers_;

    base::ScopedObservation<BrowserList, BrowserListObserver> observation_{
        this};
  };

  // To know whether the choice was made during the Profile Picker or not.
  bool choice_made_in_profile_picker_ = false;

  // Caches data backing the choice screens, shared across all of the choice
  // screens dialogs shown by this profile.
  // It gets lazily populated the first time something tries to get it.
  std::unique_ptr<search_engines::ChoiceScreenData> choice_screen_data_;

  // The `KeyedService` lifetime is expected to exceed the profile's.
  const raw_ref<Profile> profile_;
  const raw_ref<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;
  const raw_ref<TemplateURLService> template_url_service_;

  // Maintains the registration of browser dialogs for this profile.
  // TODO(crbug.com/347223092): Investigate whether it should be destroyed as a
  // way to more strictly prevent any re-registration once a choice is made.
  BrowserRegistry browser_registry_{*this};

  base::WeakPtrFactory<SearchEngineChoiceDialogService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_SERVICE_H_
