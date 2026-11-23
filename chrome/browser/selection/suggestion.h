// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_SUGGESTION_H_
#define CHROME_BROWSER_SELECTION_SUGGESTION_H_

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/selection/mojom/action.mojom-forward.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace optimization_guide::proto {
enum SmartSelectionToolId : int;
}  // namespace optimization_guide::proto

namespace selection {

class Suggestion {
 public:
  using ToolId = optimization_guide::proto::SmartSelectionToolId;

  // Binds an endpoint the surface supplied for `Interface`.
  template <typename Interface>
  using ReceiverBinder =
      base::RepeatingCallback<void(mojo::PendingAssociatedReceiver<Interface>)>;

  Suggestion();
  virtual ~Suggestion();

  // Returns the label for the suggestion.
  virtual const std::u16string& GetLabel() const = 0;

  // Returns the tool ID that produced this suggestion.
  virtual ToolId GetToolId() const = 0;

  // Called when the suggestion is presented.
  virtual void OnSuggestionPresented() = 0;

  // Called when the user accepts a suggestion associated with this tool.
  virtual void OnSuggestionExecuted() = 0;

  // Returns what executing this suggestion should do to the surface that
  // offered it.
  virtual mojom::ActionPtr GetAction() const = 0;

  // Hands `endpoint` to the binder registered by `SetInterface()`, then runs
  // `OnSuggestionExecuted()`. If nothing is registered, `endpoint` is invalid,
  // or its interface name does not match the registered one, `endpoint` is
  // dropped.
  void Execute(mojo::GenericPendingAssociatedReceiver endpoint);

  std::string_view interface_name() const { return interface_name_; }

 protected:
  // Registers the binder that receives the endpoint supplied when this
  // suggestion is executed.
  template <typename Interface>
  void SetInterface(
      const std::type_identity_t<ReceiverBinder<Interface>>& binder) {
    CHECK(!binder_);
    interface_name_ = Interface::Name_;
    binder_ = base::BindRepeating(
        [](const ReceiverBinder<Interface>& binder,
           mojo::GenericPendingAssociatedReceiver endpoint) {
          // `As()` returns an empty handle if name mismatches with the
          // binder.
          if (auto receiver = endpoint.As<Interface>()) {
            binder.Run(std::move(receiver));
          }
        },
        binder);
  }

 private:
  base::RepeatingCallback<void(mojo::GenericPendingAssociatedReceiver)> binder_;

  // Fully qualified name of the interface registered by `SetInterface()`, or
  // empty if nothing is registered. Mojom's interface name has static storage
  // duration.
  std::string_view interface_name_;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_SUGGESTION_H_

