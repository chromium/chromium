// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"

ExtensionConsoleErrorObserver::ExtensionConsoleErrorObserver(
    content::BrowserContext* context,
    const char* extension_id)
    : context_(context), extension_id_(extension_id) {
  auto* pm = extensions::ProcessManager::Get(context_);
  CHECK(pm);
  pm->AddObserver(this);

  // In case the extension already loaded.
  extensions::ExtensionHost* host =
      pm->GetBackgroundHostForExtension(extension_id_);
  if (host)
    OnBackgroundHostCreated(host);
}

ExtensionConsoleErrorObserver ::~ExtensionConsoleErrorObserver() {
  // Intentionally skip removal of this instance from ProcessManager; it leads
  // to errors in DependencyManager.
}

bool ExtensionConsoleErrorObserver::HasErrorsOrWarnings() {
  return console_observer_ && !console_observer_->messages().empty();
}

std::string ExtensionConsoleErrorObserver::GetErrorOrWarningAt(
    size_t index) const {
  return console_observer_ ? console_observer_->GetMessageAt(index)
                           : std::string();
}

size_t ExtensionConsoleErrorObserver::GetErrorsAndWarningsCount() const {
  return console_observer_ ? console_observer_->messages().size() : 0U;
}

void ExtensionConsoleErrorObserver::OnBackgroundHostCreated(
    extensions::ExtensionHost* host) {
  if (host->extension_id() != extension_id_)
    return;

  console_observer_ = std::make_unique<content::WebContentsConsoleObserver>(
      host->host_contents());

  auto filter =
      [](const content::WebContentsConsoleObserver::Message& message) {
        return message.log_level ==
                   blink::mojom::ConsoleMessageLevel::kWarning ||
               message.log_level == blink::mojom::ConsoleMessageLevel::kError;
      };
  console_observer_->SetFilter(base::BindRepeating(filter));
}
