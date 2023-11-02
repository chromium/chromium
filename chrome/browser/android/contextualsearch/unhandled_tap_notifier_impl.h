// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_UNHANDLED_TAP_NOTIFIER_IMPL_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_UNHANDLED_TAP_NOTIFIER_IMPL_H_

#include "chrome/browser/android/contextualsearch/unhandled_tap_web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom.h"

namespace contextual_search {

// Implements a Mojo service endpoint for the mojo unhandled-tap notifier
// message.
// TODO(donnd): remove this as part of removal of all JS APIs for Contextual
// Search since their primary need was for translations which are now handled
// directly within the Bar.
class UnhandledTapNotifierImpl : public blink::mojom::UnhandledTapNotifier {
 public:
  // Creates an implementation that will scale tap locations by the given
  // |scale_factor| (when needed) and call the given |callback| when Mojo
  // ShowUnhandledTapUIIfNeeded messages are received for the
  // unhandled_tap_notifier service.
  explicit UnhandledTapNotifierImpl(UnhandledTapCallback callback);

  UnhandledTapNotifierImpl(const UnhandledTapNotifierImpl&) = delete;
  UnhandledTapNotifierImpl& operator=(const UnhandledTapNotifierImpl&) = delete;

  ~UnhandledTapNotifierImpl() override;

  // Mojo UnhandledTapNotifier implementation.
  void ShowUnhandledTapUIIfNeeded(
      blink::mojom::UnhandledTapInfoPtr unhandled_tap_info) override;

 private:
  // Callback to call when an unhandled tap notification takes place.
  UnhandledTapCallback unhandled_tap_callback_;
};

// static
void CreateUnhandledTapNotifierImpl(
    UnhandledTapCallback callback,
    mojo::PendingReceiver<blink::mojom::UnhandledTapNotifier> receiver);

}  // namespace contextual_search

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_UNHANDLED_TAP_NOTIFIER_IMPL_H_
