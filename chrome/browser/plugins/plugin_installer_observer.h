// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_OBSERVER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_OBSERVER_H_

#include <string>

class PluginInstaller;

class PluginInstallerObserver {
 public:
  explicit PluginInstallerObserver(PluginInstaller* installer);
  virtual ~PluginInstallerObserver();

 protected:
  PluginInstaller* installer() const { return installer_; }

 private:
  friend class PluginInstaller;

  virtual void DownloadFinished();

  // Weak pointer; Owned by PluginFinder, which is a singleton.
  PluginInstaller* installer_;
};

// A WeakPluginInstallerObserver is like a weak pointer to the installer, in the
// sense that if only weak observers are left, we don't need to show
// installation UI anymore.
class WeakPluginInstallerObserver : public PluginInstallerObserver {
 public:
  explicit WeakPluginInstallerObserver(PluginInstaller* installer);
  ~WeakPluginInstallerObserver() override;

 private:
  friend class PluginInstaller;

  virtual void OnlyWeakObserversLeft();
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_OBSERVER_H_
