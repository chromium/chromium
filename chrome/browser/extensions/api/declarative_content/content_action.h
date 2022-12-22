// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_ACTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_ACTION_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/mojom/host_id.mojom-forward.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

class Extension;
class ExtensionUserScriptLoader;

// Base class for all ContentActions of the Declarative Content API.
//
// For example, given the sample code at
// https://developer.chrome.com/extensions/declarativeContent#rules, the entity
// rule1['actions'][0] is represented by a ContentAction subclass.
class ContentAction {
 public:
  struct ApplyInfo {
    raw_ptr<const Extension> extension;
    raw_ptr<content::BrowserContext> browser_context;
    raw_ptr<content::WebContents> tab;
    int priority;
  };

  virtual ~ContentAction();

  // Applies or reverts this ContentAction on a particular tab for a particular
  // extension.  Revert exists to keep the actions up to date as the page
  // changes.  Reapply exists to reapply changes to a new page, even if the
  // previous page also matched relevant conditions.
  virtual void Apply(const ApplyInfo& apply_info) const = 0;
  virtual void Reapply(const ApplyInfo& apply_info) const = 0;
  virtual void Revert(const ApplyInfo& apply_info) const = 0;

  // Factory method that instantiates a concrete ContentAction implementation
  // according to |json_action|, the representation of the ContentAction as
  // received from the extension API.  Sets |error| and returns NULL in case of
  // an error.
  static std::unique_ptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Value::Dict& json_action_dict,
      std::string* error);

  static void SetAllowInvisibleIconsForTest(bool value);

 protected:
  ContentAction();
};

// Action that injects a content script.
class RequestContentScript : public ContentAction,
                             public UserScriptLoader::Observer {
 public:
  struct ScriptData;

  RequestContentScript(content::BrowserContext* browser_context,
                       const Extension* extension,
                       const ScriptData& script_data);

  RequestContentScript(const RequestContentScript&) = delete;
  RequestContentScript& operator=(const RequestContentScript&) = delete;

  ~RequestContentScript() override;

  static std::unique_ptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Value::Dict* dict,
      std::string* error);

  static bool InitScriptData(const base::Value::Dict* dict,
                             std::string* error,
                             ScriptData* script_data);

  // Implementation of ContentAction:
  void Apply(const ApplyInfo& apply_info) const override;
  void Reapply(const ApplyInfo& apply_info) const override;
  void Revert(const ApplyInfo& apply_info) const override;

 private:
  void InitScript(const mojom::HostID& host_id,
                  const Extension* extension,
                  const ScriptData& script_data);

  void AddScript();

  void InstructRenderProcessToInject(content::WebContents* contents,
                                     const Extension* extension) const;

  // UserScriptLoader::Observer:
  void OnScriptsLoaded(UserScriptLoader* loader,
                       content::BrowserContext* browser_context) override;
  void OnUserScriptLoaderDestroyed(UserScriptLoader* loader) override;

  UserScript script_;
  raw_ptr<ExtensionUserScriptLoader> script_loader_ = nullptr;
  base::ScopedObservation<UserScriptLoader, UserScriptLoader::Observer>
      scoped_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_ACTION_H_
