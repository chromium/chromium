// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/version.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "url/gurl.h"

class PluginInstallerObserver;
class WeakPluginInstallerObserver;

namespace content {
class WebContents;
}

class PluginInstaller {
 public:
  PluginInstaller();
  ~PluginInstaller();

  void AddObserver(PluginInstallerObserver* observer);
  void RemoveObserver(PluginInstallerObserver* observer);

  void AddWeakObserver(WeakPluginInstallerObserver* observer);
  void RemoveWeakObserver(WeakPluginInstallerObserver* observer);

  // Opens the download URL in a new tab.
  void OpenDownloadURL(const GURL& plugin_url,
                       content::WebContents* web_contents);

 private:
  FRIEND_TEST_ALL_PREFIXES(PluginInstallerTest,
                           StartInstalling_SuccessfulDownload);
  FRIEND_TEST_ALL_PREFIXES(PluginInstallerTest, StartInstalling_FailedStart);
  FRIEND_TEST_ALL_PREFIXES(PluginInstallerTest, StartInstalling_Interrupted);

  base::ObserverList<PluginInstallerObserver>::Unchecked observers_;
  int strong_observer_count_;
  base::ObserverList<WeakPluginInstallerObserver>::Unchecked weak_observers_;
  DISALLOW_COPY_AND_ASSIGN(PluginInstaller);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_
