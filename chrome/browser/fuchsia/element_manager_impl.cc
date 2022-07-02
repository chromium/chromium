// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fuchsia/element_manager_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/browser_list.h"

namespace {
bool HaveBrowser() {
  return !BrowserList::GetInstance()->empty();
}
}  // namespace

bool ElementManagerImpl::AnnotationKeyCompare::operator()(
    const fuchsia::element::AnnotationKey& lhs,
    const fuchsia::element::AnnotationKey& rhs) const {
  return std::tie(lhs.namespace_, lhs.value) <
         std::tie(rhs.namespace_, rhs.value);
}

ElementManagerImpl::ElementManagerImpl(
    sys::OutgoingDirectory* outgoing_directory,
    NewProposalCallback callback)
    : binding_(outgoing_directory, this), callback_(std::move(callback)) {
  DCHECK(callback_);
  BrowserList::AddObserver(this);
}

ElementManagerImpl::~ElementManagerImpl() {
  BrowserList::RemoveObserver(this);
}

std::vector<fuchsia::element::Annotation> ElementManagerImpl::GetAnnotations() {
  std::vector<fuchsia::element::Annotation> annotations;
  for (const auto& annotation : annotations_) {
    annotations.push_back(fidl::Clone(annotation.second));
  }
  return annotations;
}

void ElementManagerImpl::ProposeElement(
    fuchsia::element::Spec spec,
    fidl::InterfaceRequest<fuchsia::element::Controller> element_controller,
    ProposeElementCallback callback) {
  if (!spec.has_component_url() ||
      !base::EndsWith(spec.component_url(), "/chrome#meta/chrome.cm")) {
    callback(fuchsia::element::Manager_ProposeElement_Result::WithErr(
        fuchsia::element::ProposeElementError::INVALID_ARGS));
    return;
  }

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  if (!callback_.Run(command_line)) {
    callback(fuchsia::element::Manager_ProposeElement_Result::WithErr(
        fuchsia::element::ProposeElementError::INVALID_ARGS));
  }

  if (element_controller) {
    controller_bindings_.AddBinding(this, std::move(element_controller));
  }
  annotations_.clear();
  for (auto& annotation : *spec.mutable_annotations()) {
    auto key = fidl::Clone(annotation.key);
    annotations_.insert_or_assign(std::move(key), std::move(annotation));
  }
  callback(fuchsia::element::Manager_ProposeElement_Result::WithResponse({}));
}

void ElementManagerImpl::UpdateAnnotations(
    std::vector<fuchsia::element::Annotation> annotations_to_set,
    std::vector<fuchsia::element::AnnotationKey> annotations_to_delete,
    UpdateAnnotationsCallback callback) {
  for (const auto& key : annotations_to_delete) {
    annotations_.erase(key);
  }
  for (auto& annotation : annotations_to_set) {
    auto key = fidl::Clone(annotation.key);
    annotations_.insert_or_assign(std::move(key), std::move(annotation));
  }
  callback(fuchsia::element::AnnotationController_UpdateAnnotations_Result::
               WithResponse({}));
}

void ElementManagerImpl::GetAnnotations(GetAnnotationsCallback callback) {
  fuchsia::element::AnnotationController_GetAnnotations_Response response(
      GetAnnotations());
  callback(fuchsia::element::AnnotationController_GetAnnotations_Result::
               WithResponse(std::move(response)));
}

void ElementManagerImpl::OnBrowserRemoved(Browser* browser) {
  // If the browser was the last, clear all active bindings to notify the shell.
  bool have_browser =
      have_browser_for_test_ ? have_browser_for_test_.Run() : HaveBrowser();
  if (!have_browser) {
    controller_bindings_.CloseAll();
  }
}
