// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_AVAILABLE_OFFLINE_CONTENT_PROVIDER_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_AVAILABLE_OFFLINE_CONTENT_PROVIDER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

class Profile;

namespace offline_items_collection {
class OfflineContentAggregator;
struct OfflineItem;
}  // namespace offline_items_collection

namespace android {

// Provides access to items available while offline.
class AvailableOfflineContentProvider
    : public chrome::mojom::AvailableOfflineContentProvider {
 public:
  // Public for testing.
  explicit AvailableOfflineContentProvider(int render_process_host_id);

  AvailableOfflineContentProvider(const AvailableOfflineContentProvider&) =
      delete;
  AvailableOfflineContentProvider& operator=(
      const AvailableOfflineContentProvider&) = delete;

  ~AvailableOfflineContentProvider() override;

  // chrome::mojom::AvailableOfflineContentProvider methods.
  void List(ListCallback callback) override;
  void LaunchItem(const std::string& item_id,
                  const std::string& name_space) override;
  void LaunchDownloadsPage(bool open_prefetched_articles_tab) override;
  void ListVisibilityChanged(bool is_visible) override;

  static void Create(
      int render_process_host_id,
      mojo::PendingReceiver<chrome::mojom::AvailableOfflineContentProvider>
          receiver);

 private:
  void ListFinalize(
      AvailableOfflineContentProvider::ListCallback callback,
      offline_items_collection::OfflineContentAggregator* aggregator,
      const std::vector<offline_items_collection::OfflineItem>& all_items);

  Profile* GetProfile();

  void SetSelfOwnedReceiver(const mojo::SelfOwnedReceiverRef<
                            chrome::mojom::AvailableOfflineContentProvider>&
                                provider_self_owned_receiver);
  void CloseSelfOwnedReceiverIfNeeded();

  const int render_process_host_id_;
  mojo::SelfOwnedReceiverRef<chrome::mojom::AvailableOfflineContentProvider>
      provider_self_owned_receiver_;

  base::WeakPtrFactory<AvailableOfflineContentProvider> weak_ptr_factory_{this};
};

}  // namespace android

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_AVAILABLE_OFFLINE_CONTENT_PROVIDER_H_
