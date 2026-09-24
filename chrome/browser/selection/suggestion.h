// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_SUGGESTION_H_
#define CHROME_BROWSER_SELECTION_SUGGESTION_H_

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/selection/mojom/action.mojom-forward.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace selection {

// Area of interest for the suggested service to examine.
struct AreaOfInterest {
  AreaOfInterest();
  AreaOfInterest(const AreaOfInterest&);
  AreaOfInterest& operator=(const AreaOfInterest&);
  AreaOfInterest(AreaOfInterest&&);
  AreaOfInterest& operator=(AreaOfInterest&&);
  ~AreaOfInterest();

  optimization_guide::proto::AnnotatedPageContent apc;
  SkBitmap screenshot;
  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds;
};

class Suggestion {
 public:
  // Binds an endpoint the surface supplied for `Interface`.
  template <typename Interface>
  using ReceiverBinder =
      base::RepeatingCallback<void(mojo::PendingAssociatedReceiver<Interface>)>;

  Suggestion();
  virtual ~Suggestion();

  // Returns the label for the suggestion.
  virtual const std::u16string& GetLabel() const = 0;

  // Called when the suggestion is presented.
  virtual void OnSuggestionPresented() = 0;

  // Called when the user accepts a suggestion associated with this tool.
  virtual void OnSuggestionExecuted() = 0;

  // Returns what executing this suggestion should do to the surface that
  // offered it.
  virtual mojom::ActionPtr GetAction() const = 0;

  // Hands `endpoint` to the binder registered by `SetInterface()`, then runs
  // `OnSuggestionExecuted()`. If nothing is registered, or `endpoint` is
  // invalid, `endpoint` is closed.
  void Execute(mojo::ScopedInterfaceEndpointHandle endpoint);

  std::string_view interface_name() const { return interface_name_; }

 protected:
  // Registers the binder that receives the endpoint supplied when this
  // suggestion is executed. The surface passes an untyped endpoint and never
  // names an interface, so it cannot ask to be bound to a different one.
  template <typename Interface>
  void SetInterface(
      const std::type_identity_t<ReceiverBinder<Interface>>& binder) {
    CHECK(!binder_);
    interface_name_ = Interface::Name_;
    binder_ = base::BindRepeating(
        [](const ReceiverBinder<Interface>& binder,
           mojo::ScopedInterfaceEndpointHandle endpoint) {
          binder.Run(
              mojo::PendingAssociatedReceiver<Interface>(std::move(endpoint)));
        },
        binder);
  }

 private:
  base::RepeatingCallback<void(mojo::ScopedInterfaceEndpointHandle)> binder_;

  // Fully qualified name of the interface registered by `SetInterface()`, or
  // empty if nothing is registered. Mojom's interface name has static storage
  // duration.
  std::string_view interface_name_;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_SUGGESTION_H_

