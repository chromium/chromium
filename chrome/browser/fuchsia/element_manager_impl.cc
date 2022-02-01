// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fuchsia/element_manager_impl.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/chrome_browser_main.h"

ElementManagerImpl::ElementManagerImpl(
    sys::OutgoingDirectory* outgoing_directory,
    NewProposalCallback callback)
    : binding_(outgoing_directory, this), callback_(std::move(callback)) {
  DCHECK(callback_);
}

ElementManagerImpl::~ElementManagerImpl() = default;

void ElementManagerImpl::ProposeElement(
    fuchsia::element::Spec spec,
    fidl::InterfaceRequest<fuchsia::element::Controller> element_controller,
    ProposeElementCallback callback) {
  fuchsia::element::Manager_ProposeElement_Result result;

  if (spec.component_url() !=
      "fuchsia-pkg://fuchsia.com/chrome#meta/chrome.cm") {
    result.set_err(fuchsia::element::ProposeElementError::INVALID_ARGS);
    callback(std::move(result));
    return;
  }

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  if (callback_.Run(command_line)) {
    result.set_response({});
  } else {
    result.set_err(fuchsia::element::ProposeElementError::INVALID_ARGS);
  }
  callback(std::move(result));
}
