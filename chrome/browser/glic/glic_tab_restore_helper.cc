// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_tab_restore_data.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace glic {

namespace {
// Keys for extra_data map.
// Legacy keys for backward compatibility (and simplicity for bound instance).
constexpr char kGlicInstanceIdKey[] = "glic.instance_id";
constexpr char kGlicConversationIdKey[] = "glic.conversation_id";
constexpr char kGlicPanelOpenKey[] = "glic.panel_open";
// New key for pinned instances list (JSON).
constexpr char kGlicPinnedInstancesKey[] = "glic.pinned_instances";

constexpr char kInstanceIdKey[] = "instance_id";
constexpr char kConversationIdKey[] = "conversation_id";
}  // namespace

void PopulateGlicExtraData(content::WebContents* web_contents,
                           std::map<std::string, std::string>* extra_data) {
  if (!base::FeatureList::IsEnabled(features::kGlicTabRestoration)) {
    return;
  }

  if (!web_contents) {
    return;
  }

  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  if (!tab) {
    return;
  }

  auto* helper = GlicInstanceHelper::From(tab);
  if (helper) {
    if (auto instance_id = helper->GetInstanceId()) {
      (*extra_data)[kGlicInstanceIdKey] = instance_id->AsLowercaseString();
    }
    if (auto conversation_id = helper->GetConversationId()) {
      (*extra_data)[kGlicConversationIdKey] = *conversation_id;
    }

    auto pinned_instances = helper->GetPinnedInstances();
    if (!pinned_instances.empty()) {
      base::ListValue list;
      for (const auto* instance : pinned_instances) {
        if (!instance) {
          continue;
        }
        base::DictValue dict;
        dict.Set(kInstanceIdKey, instance->id().AsLowercaseString());
        if (auto conv_id = instance->conversation_id()) {
          dict.Set(kConversationIdKey, *conv_id);
        }
        list.Append(std::move(dict));
      }
      std::string json;
      if (base::JSONWriter::Write(list, &json)) {
        (*extra_data)[kGlicPinnedInstancesKey] = json;
      }
    }
  }

  if (GlicSidePanelCoordinator::IsGlicSidePanelActive(tab)) {
    (*extra_data)[kGlicPanelOpenKey] = "true";
  }
}

void RestoreGlicStateFromExtraData(
    content::WebContents* web_contents,
    const std::map<std::string, std::string>& extra_data) {
  if (!base::FeatureList::IsEnabled(features::kGlicTabRestoration)) {
    return;
  }

  GlicRestoredState state;
  bool has_state = false;

  // Restore bound instance
  auto it_instance = extra_data.find(kGlicInstanceIdKey);
  if (it_instance != extra_data.end()) {
    state.bound_instance.instance_id = it_instance->second;
    has_state = true;
  }

  auto it_conversation = extra_data.find(kGlicConversationIdKey);
  if (it_conversation != extra_data.end()) {
    state.bound_instance.conversation_id = it_conversation->second;
  }

  auto it_panel_open = extra_data.find(kGlicPanelOpenKey);
  if (it_panel_open != extra_data.end() && it_panel_open->second == "true") {
    state.side_panel_open = true;
  }

  // Restore pinned instances
  auto it_pinned = extra_data.find(kGlicPinnedInstancesKey);
  if (it_pinned != extra_data.end()) {
    auto result =
        base::JSONReader::Read(it_pinned->second, base::JSON_PARSE_RFC);
    if (result && result->is_list()) {
      for (const auto& item : result->GetList()) {
        if (!item.is_dict()) {
          continue;
        }
        const auto& dict = item.GetDict();
        const std::string* id = dict.FindString(kInstanceIdKey);
        const std::string* conv_id = dict.FindString(kConversationIdKey);
        if (id) {
          GlicRestoredState::InstanceInfo info;
          info.instance_id = *id;
          if (conv_id) {
            info.conversation_id = *conv_id;
          }
          state.pinned_instances.push_back(std::move(info));
          has_state = true;
        }
      }
    }
  }

  if (has_state) {
    GlicTabRestoreData::CreateForWebContents(web_contents, std::move(state));
  }
}

}  // namespace glic
