// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_function.h"

#include <utility>

#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_function_dispatcher.h"

ChromeAsyncExtensionFunction::ChromeAsyncExtensionFunction()
    : chrome_details_(this) {}

ChromeAsyncExtensionFunction::~ChromeAsyncExtensionFunction() {}

Profile* ChromeAsyncExtensionFunction::GetProfile() const {
  return Profile::FromBrowserContext(context_);
}

void ChromeAsyncExtensionFunction::SetError(const std::string& error) {
  error_ = error;
}

const std::string& ChromeAsyncExtensionFunction::GetError() const {
  return error_.empty() ? ExtensionFunction::GetError() : error_;
}

void ChromeAsyncExtensionFunction::SendResponse(bool success) {
  ResponseValue response;
  if (success) {
    response = ArgumentList(std::move(results_));
  } else {
    response = results_ ? ErrorWithArguments(std::move(results_), error_)
                        : Error(error_);
  }
  Respond(std::move(response));
}

void ChromeAsyncExtensionFunction::SetResult(
    std::unique_ptr<base::Value> result) {
  results_.reset(new base::ListValue());
  results_->Append(std::move(result));
}

void ChromeAsyncExtensionFunction::SetResultList(
    std::unique_ptr<base::ListValue> results) {
  results_ = std::move(results);
}

ExtensionFunction::ResponseAction ChromeAsyncExtensionFunction::Run() {
  if (RunAsync())
    return RespondLater();
  // TODO(devlin): Track these down and eliminate them if possible. We
  // shouldn't return results and an error.
  if (results_)
    return RespondNow(ErrorWithArguments(std::move(results_), error_));
  return RespondNow(Error(error_));
}

// static
bool ChromeAsyncExtensionFunction::ValidationFailure(
    ChromeAsyncExtensionFunction* function) {
  return false;
}
