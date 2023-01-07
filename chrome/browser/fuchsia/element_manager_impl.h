// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FUCHSIA_ELEMENT_MANAGER_IMPL_H_
#define CHROME_BROWSER_FUCHSIA_ELEMENT_MANAGER_IMPL_H_

#include <fuchsia/element/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>

#include "base/containers/flat_map.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/fuchsia_component_support/annotations_manager.h"

namespace base {
class CommandLine;
}  // namespace base

namespace sys {
class OutgoingDirectory;
}  // namespace sys

class ElementManagerImpl final : public fuchsia::element::Manager,
                                 public fuchsia::element::Controller,
                                 public BrowserListObserver {
 public:
  // Implement this callback to handle notifications to start a new element. The
  // callback will receive the command line needed to start the new window(s).
  // Return true if the command line will be handled within the current browser
  // instance or false if not (i.e., because the current process is shutting
  // down).
  using NewProposalCallback =
      base::RepeatingCallback<bool(const base::CommandLine& command_line)>;

  // Implement this callback to let the manager knows whether there is at least
  // one browser window. This is expected to only be set by tests, the Manager
  // will use BrowserList if this is not set.
  using HaveBrowserCallback = base::RepeatingCallback<bool()>;

  ElementManagerImpl(sys::OutgoingDirectory* outgoing_directory,
                     NewProposalCallback callback);
  ~ElementManagerImpl() override;

  ElementManagerImpl(const ElementManagerImpl&) = delete;
  ElementManagerImpl& operator=(const ElementManagerImpl&) = delete;

  fuchsia_component_support::AnnotationsManager& annotations_manager() {
    return annotations_manager_;
  }

  // fuchsia::element::Manager implementation
  void ProposeElement(
      fuchsia::element::Spec spec,
      fidl::InterfaceRequest<fuchsia::element::Controller> element_controller,
      ProposeElementCallback callback) override;

  void set_have_browser_callback_for_test(HaveBrowserCallback callback) {
    have_browser_for_test_ = std::move(callback);
  }

 private:
  struct AnnotationKeyCompare {
    bool operator()(const fuchsia::element::AnnotationKey& lhs,
                    const fuchsia::element::AnnotationKey& rhs) const;
  };

  // fuchsia::element::Controller implementation
  void UpdateAnnotations(
      std::vector<fuchsia::element::Annotation> annotations_to_set,
      std::vector<fuchsia::element::AnnotationKey> annotations_to_delete,
      UpdateAnnotationsCallback callback) override;
  void GetAnnotations(GetAnnotationsCallback callback) override;

  // BrowserListObserver
  void OnBrowserRemoved(Browser* browser) override;

  base::ScopedServiceBinding<fuchsia::element::Manager> binding_;
  const NewProposalCallback new_proposal_callback_;

  fidl::BindingSet<fuchsia::element::Controller> controller_bindings_;
  fuchsia_component_support::AnnotationsManager annotations_manager_;

  HaveBrowserCallback have_browser_for_test_;
};

#endif  // CHROME_BROWSER_FUCHSIA_ELEMENT_MANAGER_IMPL_H_
