// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PRECONNECTOR_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PRECONNECTOR_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/lru_cache.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/public/browser/preconnect_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/socket/next_proto.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace android_webview {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Note: When prewarming is initiated, AwPreconnector speculatively requests and
// pins a dictionary preload handle in the Network Service via Mojo before
// knowing whether a shared dictionary actually exists on disk for that URL.
// Therefore, `kPrewarmed` is recorded regardless of whether a matching
// dictionary is found in SQLite.
//
// LINT.IfChange(AwPrewarmDictionaryEvent)
enum class AwPrewarmDictionaryEvent {
  // A Mojo request was issued to NetworkContext to prewarm shared dictionary
  // metadata for a newly preconnected URL, and a handle was pinned in the LRU
  // cache.
  kPrewarmed = 0,
  // The preconnected URL was already pinned in the LRU cache; its active
  // session count was incremented and its 30-second expiry timer was refreshed.
  kRefreshed = 1,
  // The LRU cache reached capacity
  // (`kWebViewPrewarmDictionaryOnPreconnectMaxEntries`), evicting the least
  // recently used prewarmed dictionary handle.
  kEvicted = 2,
  // The 30-second fallback timer fired without connection closure, releasing
  // the pinned dictionary handle.
  kExpired = 3,
  // All active preconnect sessions for this URL closed, releasing the pinned
  // dictionary handle (in `events_only` or `hybrid` cleanup mode).
  kClosedSessionClosed = 4,
  // The preconnected socket failed, releasing the pinned dictionary handle
  // (in `events_only` or `hybrid` cleanup mode).
  kClosedConnectionFailed = 5,
  // The Mojo remote pipe to NetworkContext was disconnected (e.g. on Android
  // memory pressure or Network Service crash), causing immediate cleanup.
  kDisconnected = 6,
  kMaxValue = kDisconnected,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AwPrewarmDictionaryEvent)

// Holds a content::PreconnectManager, owning it for the lifetime of the Profile
// and exposing via the Java AwPreconnector (which this class also owns).
// Lifetime: Profile
class AwPreconnector : public content::PreconnectManager::Delegate,
                       public network::mojom::ConnectionChangeObserverClient {
 public:
  explicit AwPreconnector(content::BrowserContext* browser_context);
  ~AwPreconnector() override;
  AwPreconnector(const AwPreconnector&) = delete;
  AwPreconnector& operator=(const AwPreconnector&) = delete;

  // Preconnects to the given URL. Returns false if the URL is invalid.
  bool Preconnect(JNIEnv* env, const GURL& url);

  base::android::ScopedJavaLocalRef<jobject> GetJavaAwPreconnector();

  // PreconnectManager::Delegate:
  void PreconnectInitiated(const GURL& url,
                           const GURL& preconnect_url) override;
  void PreconnectFinished(
      std::unique_ptr<content::PreconnectStats> stats) override;

  bool IsPreconnectEnabled() override;

  // network::mojom::ConnectionChangeObserverClient:
  void OnConnectionEstablished(
      const net::ConnectionChangeNotifier::EstablishedConnectionInfo& info)
      override;
  void OnSessionClosed(bool was_ever_used_to_create_streams) override;
  void OnNetworkEvent(net::NetworkChangeEvent event) override;
  void OnConnectionFailed() override;

  bool HasPrewarmedDictionaryForTesting(const GURL& url) const {
    return prewarmed_shared_dictionaries_.Peek(url) !=
           prewarmed_shared_dictionaries_.end();
  }

 private:
  struct PreconnectContext {
    base::TimeTicks start_time;
    GURL url;
    net::NextProto connection_info = net::NextProto::kProtoUnknown;
  };

  // Holds the Mojo remote and optional expiry timer for a prewarmed shared
  // dictionary. The Mojo `remote` keeps the dictionary metadata pinned in
  // NetworkContext (Network Service memory). Destroying this entry closes the
  // remote, which immediately unpins the dictionary in NetworkContext.
  struct PrewarmedDictionaryEntry {
    PrewarmedDictionaryEntry();
    ~PrewarmedDictionaryEntry();

    mojo::Remote<network::mojom::PreloadedSharedDictionaryInfoHandle> remote;
    base::OneShotTimer timer;
    size_t active_session_count = 0;
  };

  content::PreconnectManager& GetPreconnectManager();

  // Issues a Mojo request to NetworkContext to prewarm dictionary metadata for
  // `url` and registers it in `prewarmed_shared_dictionaries_`.
  void PrewarmDictionary(const GURL& url);

  // Called when a prewarmed dictionary entry's 30-second expiry timer fires.
  // Removes the entry from `prewarmed_shared_dictionaries_`, closing the Mojo
  // remote and freeing network memory.
  void OnDictionaryExpired(const GURL& url);

  // Called when the Mojo pipe to NetworkContext disconnects (e.g. on
  // Network Service memory pressure or crash). Removes the entry.
  void OnDictionaryDisconnected(const GURL& url);

  // Called when a connection session closes or fails. Decrements the active
  // session count for `url` and cleans up the prewarmed dictionary entry when
  // all active sessions for that URL have terminated (if cleanup mode permits).
  void MaybeCleanupPrewarmedDictionary(const GURL& url, bool is_session_closed);

  using PrewarmedDictionaryMap =
      base::LRUCache<GURL, std::unique_ptr<PrewarmedDictionaryEntry>>;

  // Removes a prewarmed dictionary entry from `prewarmed_shared_dictionaries_`
  // and records the corresponding lifecycle event.
  void CleanupPrewarmedDictionary(PrewarmedDictionaryMap::iterator it,
                                  AwPrewarmDictionaryEvent event);

  const raw_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<content::PreconnectManager> preconnect_manager_;

  mojo::ReceiverSet<network::mojom::ConnectionChangeObserverClient,
                    PreconnectContext>
      receivers_;

  // LRU cache mapping preconnected URLs to their prewarmed dictionary entries.
  // Automatically evicts the least recently used entry when max capacity
  // (`kWebViewPrewarmDictionaryOnPreconnectMaxEntries`) is exceeded.
  PrewarmedDictionaryMap prewarmed_shared_dictionaries_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  base::WeakPtrFactory<AwPreconnector> weak_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PRECONNECTOR_H_
