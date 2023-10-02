// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_H_
#define CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"

class Browser;
class BrowserListObserver;

// Service handling the Search Engine Choice dialog.
class SearchEngineChoiceService : public KeyedService {
 public:
  SearchEngineChoiceService(Profile& profile,
                            TemplateURLService& template_url_service);
  ~SearchEngineChoiceService() override;

  // Informs the service that a Search Engine Choice dialog has been opened
  // for `browser`.
  // Virtual to be able to mock in tests.
  virtual void NotifyDialogOpened(Browser* browser,
                                  base::OnceClosure close_dialog_callback);

  // This function is called when the user makes a search engine choice. It
  // closes the dialogs that are open on other browser windows that
  // have the same profile as the one on which the choice was made and sets the
  // corresponding preferences.
  // `prepopulate_id` is the `prepopulate_id` of the search engine found in
  // `components/search_engines/template_url_data.h`. It will always be > 0.
  // Virtual to be able to mock in tests.
  virtual void NotifyChoiceMade(int prepopulate_id);

  // Informs the service that a Search Engine Choice dialog has been closed for
  // `browser`.
  void NotifyDialogClosed(Browser* browser);

  // Returns whether a Search Engine Choice dialog is currently open or not for
  // `browser`.
  bool IsShowingDialog(Browser* browser);

  // Returns whether the Search Engine Choice dialog can be shown or not.
  // This will return false if the dialog is currently showing.
  bool CanShowDialog(Browser& browser);

  // Returns whether the dialog should be displayed over the passed URL.
  bool IsUrlSuitableForDialog(GURL url);

  // Returns whether the Search Engine Choice dialog is either shown or
  // pending to be shown.
  bool HasPendingDialog(Browser& browser);

  // Returns whether the user has already made a choice or not.
  bool HasUserMadeChoice() const;

  // Returns whether the user made the search engine choice during the first run
  // experience.
  bool WasChoiceMadeInFRE() const;

  // Returns the list of search engines.
  // The search engine details returned by this function will be the canonical
  // ones and will not be affected by changes in search engine details from the
  // settings page.
  // Virtual to be able to mock in tests.
  virtual std::vector<std::unique_ptr<TemplateURLData>> GetSearchEngines();

  // Disables the display of the Search Engine Choice dialog for testing. When
  // `dialog_disabled` is true, `CanShowDialog` will return false.
  // NOTE: This is set to true in InProcessBrowserTest::SetUp, disabling the
  // dialog for those tests. If you set this outside of that context, you should
  // ensure it is reset at the end of your test.
  static void SetDialogDisabledForTests(bool dialog_disabled);

  // Registers the local state preferences used by the search engine choice
  // screen.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  // Observes the BrowserList to make sure that closed browsers are correctly
  // removed from our set of browser pointers. This ensures that we don't get
  // dangling pointers.
  class BrowserObserver : public BrowserListObserver {
   public:
    explicit BrowserObserver(SearchEngineChoiceService& service);
    ~BrowserObserver() override;

    void OnBrowserRemoved(Browser* browser) override;

   private:
    raw_ref<SearchEngineChoiceService> search_engine_choice_service_;
    base::ScopedObservation<BrowserList, BrowserListObserver> observation_{
        this};
  };
  friend class SearchEngineChoiceServiceFactory;

  // A map of Browser windows which have an open Search Engine Choice dialog to
  // the callback that will close the browser's dialog.
  base::flat_map<Browser*, base::OnceClosure> browsers_with_open_dialogs_;

  // Observes the browser list for closed browsers.
  BrowserObserver browser_observer_{*this};

  // To know whether the choice was made during the FRE or not.
  bool choice_made_in_fre_ = false;

  // The `KeyedService` lifetime is expected to exceed the profile's.
  const raw_ref<Profile> profile_;
  const raw_ref<TemplateURLService> template_url_service_;
  base::WeakPtrFactory<SearchEngineChoiceService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_H
