// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_IMPL_H_

#include <memory>

#include "base/containers/span.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class BrowserWindowInterface;

namespace ash {

class BrowserDelegate;
class BrowserDelegateImpl;

class BrowserControllerImpl : public BrowserController,
                              public BrowserCollectionObserver,
                              public TabStripModelObserver {
 public:
  BrowserControllerImpl();
  ~BrowserControllerImpl() override;

  // BrowserController:
  BrowserDelegate* GetDelegate(BrowserWindowInterface* bwi) override;
  BrowserDelegate* GetLastUsedBrowser() override;
  BrowserDelegate* GetLastUsedVisibleBrowser() override;
  BrowserDelegate* GetLastUsedVisibleOnTheRecordBrowser() override;
  void ForEachBrowser(BrowserOrder order,
                      base::FunctionRef<IterationDirective(BrowserDelegate&)>
                          callback) override;
  BrowserDelegate* GetBrowserForWindow(aura::Window* window) override;
  BrowserDelegate* GetBrowserForTab(content::WebContents* contents) override;
  BrowserDelegate* FindWebApp(const AccountId& account_id,
                              webapps::AppId app_id,
                              BrowserType browser_type,
                              const GURL& url) override;
  BrowserDelegate* NewTabWithPostData(const AccountId& account_id,
                                      const GURL& url,
                                      base::span<const uint8_t> post_data,
                                      std::string_view extra_headers) override;
  BrowserDelegate* CreateWebApp(const AccountId& account_id,
                                webapps::AppId app_id,
                                BrowserType browser_type,
                                const CreateParams& params) override;
  void MayCloseAllBrowsers() override;
  void MayCloseAllBrowsersAndQuit() override;
  bool IsTryingToQuit() override;
  bool HasShutdownStarted() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void AddTabObserver(TabObserver* observer) override;
  void RemoveTabObserver(TabObserver* observer) override;
  void CreateAutofillClientForWebContents(
      content::WebContents* web_contents) override;
  std::unique_ptr<views::SimpleWebView> CreateSimpleWebViewForSigninScreen(
      views::SimpleWebViewDialogDelegate* delegate) override;
  bool ShowIntentPicker(base::WeakPtr<content::WebContents> web_contents,
                        std::vector<apps::IntentPickerAppInfo> app_info,
                        bool show_stay_in_chrome,
                        bool show_remember_selection,
                        apps::IntentPickerBubbleType bubble_type,
                        base::optional_ref<const url::Origin> initiating_origin,
                        IntentPickerResponse callback) override;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  absl::flat_hash_map<BrowserWindowInterface*,
                      std::unique_ptr<BrowserDelegateImpl>>
      browsers_;
  base::ObserverList<Observer> observers_;
  base::ObserverList<TabObserver> tab_observers_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_IMPL_H_
