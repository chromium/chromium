// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"

class Browser;
class ExtensionInstallPromptShowParams;
class GlobalError;
class GlobalErrorService;

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExternalInstallManager;
class WebstoreDataFetcher;

// An error to show the user an extension has been externally installed. The
// error will automatically fetch data about the extension from the webstore (if
// possible) and will handle adding itself to the GlobalErrorService when
// initialized and removing itself from the GlobalErrorService upon
// destruction.
class ExternalInstallError : public WebstoreDataFetcherDelegate {
 public:
  // The possible types of errors to show. A menu alert adds a menu item to the
  // wrench, which spawns an extension install dialog when clicked. The bubble
  // alert also adds an item, but spawns a bubble instead (less invasive and
  // easier to dismiss).
  enum AlertType {
    BUBBLE_ALERT,
    MENU_ALERT
  };

  // The possible dialog button configurations to use in the error bubble.
  enum DefaultDialogButtonSetting {
    NOT_SPECIFIED,
    DIALOG_BUTTON_OK,
    DIALOG_BUTTON_CANCEL,
    NO_DEFAULT_DIALOG_BUTTON
  };

  ExternalInstallError(content::BrowserContext* browser_context,
                       const std::string& extension_id,
                       AlertType error_type,
                       ExternalInstallManager* manager);
  ~ExternalInstallError() override;

  void OnInstallPromptDone(ExtensionInstallPrompt::Result result);

  void DidOpenBubbleView();
  void DidCloseBubbleView();

  // Show the associated dialog. This should only be called once the dialog is
  // ready.
  void ShowDialog(Browser* browser);

  // Return the associated extension, or NULL.
  const Extension* GetExtension() const;

  const std::string& extension_id() const { return extension_id_; }
  AlertType alert_type() const { return alert_type_; }

  // Returns the setting specified by the following optional sources, by order
  // of priority:
  // 1. The webstore response's |kExternalInstallDefaultButtonKey| parameter.
  // 2. The |kExternalExtensionDefaultButtonControl| field trial parameter's
  //    |kExternalInstallDefaultButtonKey| value.
  // If not specified by either optional source, returns |NOT_SPECIFIED|.
  static DefaultDialogButtonSetting GetDefaultDialogButton(
      const base::Value& webstore_response);

  DefaultDialogButtonSetting default_dialog_button_setting() const {
    return default_dialog_button_setting_;
  }

 private:
  // WebstoreDataFetcherDelegate implementation.
  void OnWebstoreRequestFailure(const std::string& extension_id) override;
  void OnWebstoreResponseParseSuccess(
      const std::string& extension_id,
      std::unique_ptr<base::DictionaryValue> webstore_data) override;
  void OnWebstoreResponseParseFailure(const std::string& extension_id,
                                      const std::string& error) override;

  // Called when data fetching has completed (either successfully or not).
  void OnFetchComplete();

  // Called when the dialog has been successfully populated, and is ready to be
  // shown.
  void OnDialogReady(ExtensionInstallPromptShowParams* show_params,
                     const ExtensionInstallPrompt::DoneCallback& done_callback,
                     std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt);

  // Removes the error.
  void RemoveError();

  // The associated BrowserContext.
  content::BrowserContext* browser_context_;

  // The id of the external extension.
  std::string extension_id_;

  // The type of alert to show the user.
  AlertType alert_type_;

  // The dialog button configuration to use in the error bubble.
  DefaultDialogButtonSetting default_dialog_button_setting_ = NOT_SPECIFIED;

  // The owning ExternalInstallManager.
  ExternalInstallManager* manager_;

  // The associated GlobalErrorService.
  GlobalErrorService* error_service_;

  // The UI for showing the error.
  std::unique_ptr<ExtensionInstallPrompt> install_ui_;
  std::unique_ptr<ExtensionInstallPromptShowParams> install_ui_show_params_;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt_;

  // The UI for the given error, which will take the form of either a menu
  // alert or a bubble alert (depending on the |alert_type_|.
  std::unique_ptr<GlobalError> global_error_;

  // The WebstoreDataFetcher to use in order to populate the error with webstore
  // information of the extension.
  std::unique_ptr<WebstoreDataFetcher> webstore_data_fetcher_;

  base::WeakPtrFactory<ExternalInstallError> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExternalInstallError);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_H_
