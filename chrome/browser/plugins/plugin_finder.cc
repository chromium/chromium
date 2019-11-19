// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_finder.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/common/webplugininfo.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

using base::DictionaryValue;

namespace {

// Gets the full path of the plugin file as the identifier.
std::string GetLongIdentifier(const content::WebPluginInfo& plugin) {
  return plugin.path.AsUTF8Unsafe();
}

// Gets the base name of the file path as the identifier.
std::string GetIdentifier(const content::WebPluginInfo& plugin) {
  return plugin.path.BaseName().AsUTF8Unsafe();
}

// Gets the plugin group name as the plugin name if it is not empty or
// the filename without extension if the name is empty.
static base::string16 GetGroupName(const content::WebPluginInfo& plugin) {
  if (!plugin.name.empty())
    return plugin.name;

  return plugin.path.BaseName().RemoveExtension().AsUTF16Unsafe();
}

void LoadMimeTypes(bool matching_mime_types,
                   const base::DictionaryValue* plugin_dict,
                   PluginMetadata* plugin) {
  const base::ListValue* mime_types = NULL;
  std::string list_key =
      matching_mime_types ? "matching_mime_types" : "mime_types";
  if (!plugin_dict->GetList(list_key, &mime_types))
    return;

  bool success = false;
  for (auto mime_type_it = mime_types->begin();
       mime_type_it != mime_types->end(); ++mime_type_it) {
    std::string mime_type_str;
    success = mime_type_it->GetAsString(&mime_type_str);
    DCHECK(success);
    if (matching_mime_types) {
      plugin->AddMatchingMimeType(mime_type_str);
    } else {
      plugin->AddMimeType(mime_type_str);
    }
  }
}

std::unique_ptr<PluginMetadata> CreatePluginMetadata(
    const std::string& identifier,
    const base::DictionaryValue* plugin_dict) {
  std::string url;
  bool success = plugin_dict->GetString("url", &url);
  std::string help_url;
  plugin_dict->GetString("help_url", &help_url);
  base::string16 name;
  success = plugin_dict->GetString("name", &name);
  DCHECK(success);
  bool display_url = true;
  plugin_dict->GetBoolean("displayurl", &display_url);
  base::string16 group_name_matcher;
  success = plugin_dict->GetString("group_name_matcher", &group_name_matcher);
  DCHECK(success);
  std::string language_str;
  plugin_dict->GetString("lang", &language_str);
  bool plugin_is_deprecated = false;
  plugin_dict->GetBoolean("plugin_is_deprecated", &plugin_is_deprecated);

  std::unique_ptr<PluginMetadata> plugin = std::make_unique<PluginMetadata>(
      identifier, name, display_url, GURL(url), GURL(help_url),
      group_name_matcher, language_str, plugin_is_deprecated);
  const base::ListValue* versions = NULL;
  if (plugin_dict->GetList("versions", &versions)) {
    for (auto it = versions->begin(); it != versions->end(); ++it) {
      const base::DictionaryValue* version_dict = NULL;
      if (!it->GetAsDictionary(&version_dict)) {
        NOTREACHED();
        continue;
      }
      std::string version;
      success = version_dict->GetString("version", &version);
      DCHECK(success);
      std::string status_str;
      success = version_dict->GetString("status", &status_str);
      DCHECK(success);
      PluginMetadata::SecurityStatus status =
          PluginMetadata::SECURITY_STATUS_UP_TO_DATE;
      success = PluginMetadata::ParseSecurityStatus(status_str, &status);
      DCHECK(success);
      plugin->AddVersion(base::Version(version), status);
    }
  }

  LoadMimeTypes(false, plugin_dict, plugin.get());
  LoadMimeTypes(true, plugin_dict, plugin.get());
  return plugin;
}

}  // namespace

// static
PluginFinder* PluginFinder::GetInstance() {
  static PluginFinder* instance = new PluginFinder();
  return instance;
}

PluginFinder::PluginFinder() : version_(-1) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PluginFinder::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Load the built-in plugin list first. If we have a newer version stored
  // locally or download one, we will replace this one with it.
  std::unique_ptr<base::DictionaryValue> plugin_list(LoadBuiltInPluginList());

  // Gracefully handle the case where we couldn't parse the built-in plugin list
  // for some reason (https://crbug.com/388560). TODO(bauerb): Change back to a
  // DCHECK once we have gathered more data about the underlying problem.
  if (!plugin_list)
    return;

  ReinitializePlugins(plugin_list.get());
}

// static
std::unique_ptr<base::DictionaryValue> PluginFinder::LoadBuiltInPluginList() {
  base::StringPiece json_resource(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PLUGIN_DB_JSON));
  base::Optional<base::Value> value = base::JSONReader::Read(json_resource);
  if (!value)
    return nullptr;

  return base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(*value)));
}

PluginFinder::~PluginFinder() {
}

bool PluginFinder::FindPluginWithIdentifier(
    const std::string& identifier,
    PluginInstaller** installer,
    std::unique_ptr<PluginMetadata>* plugin_metadata) {
  base::AutoLock lock(mutex_);
  auto metadata_it = identifier_plugin_.find(identifier);
  if (metadata_it == identifier_plugin_.end())
    return false;
  *plugin_metadata = metadata_it->second->Clone();

  if (installer) {
    auto installer_it = installers_.find(identifier);
    if (installer_it == installers_.end())
      return false;
    *installer = installer_it->second.get();
  }
  return true;
}

void PluginFinder::ReinitializePlugins(
    const base::DictionaryValue* plugin_list) {
  base::AutoLock lock(mutex_);
  int version = 0;  // If no version is defined, we default to 0.
  const char kVersionKey[] = "x-version";
  plugin_list->GetInteger(kVersionKey, &version);
  if (version <= version_)
    return;

  version_ = version;
  identifier_plugin_.clear();

  for (base::DictionaryValue::Iterator plugin_it(*plugin_list);
      !plugin_it.IsAtEnd(); plugin_it.Advance()) {
    const base::DictionaryValue* plugin = nullptr;
    const std::string& identifier = plugin_it.key();
    if (plugin_list->GetDictionaryWithoutPathExpansion(identifier, &plugin)) {
      DCHECK(identifier_plugin_.find(identifier) == identifier_plugin_.end());
      identifier_plugin_[identifier] = CreatePluginMetadata(identifier, plugin);

      if (installers_.find(identifier) == installers_.end())
        installers_[identifier] = std::make_unique<PluginInstaller>();
    }
  }
}

std::unique_ptr<PluginMetadata> PluginFinder::GetPluginMetadata(
    const content::WebPluginInfo& plugin) {
  base::AutoLock lock(mutex_);
  for (const auto& plugin_pair : identifier_plugin_) {
    if (!plugin_pair.second->MatchesPlugin(plugin))
      continue;

    return plugin_pair.second->Clone();
  }

  // The plugin metadata was not found, create a dummy one holding
  // the name, identifier and group name only. Dummy plugin is not deprecated.
  std::string identifier = GetIdentifier(plugin);
  std::unique_ptr<PluginMetadata> metadata = std::make_unique<PluginMetadata>(
      identifier, GetGroupName(plugin), false, GURL(), GURL(), plugin.name,
      std::string(), false /* plugin_is_deprecated */);
  for (size_t i = 0; i < plugin.mime_types.size(); ++i)
    metadata->AddMatchingMimeType(plugin.mime_types[i].mime_type);

  DCHECK(metadata->MatchesPlugin(plugin));
  if (identifier_plugin_.find(identifier) != identifier_plugin_.end())
    identifier = GetLongIdentifier(plugin);

  DCHECK(identifier_plugin_.find(identifier) == identifier_plugin_.end());
  identifier_plugin_[identifier] = std::move(metadata);
  return identifier_plugin_[identifier]->Clone();
}
