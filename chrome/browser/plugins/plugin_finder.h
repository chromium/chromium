// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "ppapi/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

namespace base {
class DictionaryValue;
}

namespace content {
struct WebPluginInfo;
}

class PluginInstaller;
class PluginMetadata;

// This class should be created and initialized by calling
// |GetInstance()| and |Init()| on the UI thread.
// After that it can be safely used on any other thread.
class PluginFinder {
 public:
  static PluginFinder* GetInstance();

  PluginFinder(const PluginFinder&) = delete;
  PluginFinder& operator=(const PluginFinder&) = delete;

  // It should be called on the UI thread.
  void Init();

  // TODO(crbug.com/1187061): Refactor this to remove DictionaryValue;
  void ReinitializePlugins(const base::DictionaryValue* json_metadata);

  // Finds the plugin with the given identifier. If found, sets |installer|
  // to the corresponding PluginInstaller and |plugin_metadata| to a copy
  // of the corresponding PluginMetadata. |installer| may be null.
  bool FindPluginWithIdentifier(
      const std::string& identifier,
      PluginInstaller** installer,
      std::unique_ptr<PluginMetadata>* plugin_metadata);

  // Gets plugin metadata using |plugin|.
  std::unique_ptr<PluginMetadata> GetPluginMetadata(
      const content::WebPluginInfo& plugin);

 private:
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, JsonSyntax);
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, ReinitializePlugins);

  PluginFinder();
  ~PluginFinder();

  // TODO(crbug.com/1187061): Refactor this to remove DictionaryValue;
  // Loads the plugin information from the browser resources and parses it.
  // Returns null if the plugin list couldn't be parsed.
  static std::unique_ptr<base::DictionaryValue> LoadBuiltInPluginList();

  SEQUENCE_CHECKER(sequence_checker_);

  std::map<std::string, std::unique_ptr<PluginInstaller>> installers_;

  std::map<std::string, std::unique_ptr<PluginMetadata>> identifier_plugin_;

  // Version of the metadata information. We use this to consolidate multiple
  // sources (baked into resource and fetched from a URL), making sure that we
  // don't overwrite newer versions with older ones.
  int version_;

  // Synchronization for the above member variables is required since multiple
  // threads can be accessing them concurrently.
  base::Lock mutex_;
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_
