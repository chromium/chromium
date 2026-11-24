// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/listener_stream_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/stream_provider_delegate.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dictation {

namespace {

constexpr char kTestExtensionIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

struct DispatchedEvent {
  extensions::ExtensionId extension_id;
  std::string event_name;

  bool operator==(const DispatchedEvent&) const = default;
};

class RecordingEventRouter : public extensions::EventRouter {
 public:
  explicit RecordingEventRouter(content::BrowserContext* context)
      : extensions::EventRouter(context, nullptr) {}
  ~RecordingEventRouter() override = default;

  void BroadcastEvent(std::unique_ptr<extensions::Event> event) override {
    ADD_FAILURE() << "Unexpected BroadcastEvent for " << event->event_name;
  }

  void DispatchEventToExtension(
      const extensions::ExtensionId& extension_id,
      std::unique_ptr<extensions::Event> event) override {
    dispatched_events_.push_back({extension_id, event->event_name});
  }

  const std::vector<DispatchedEvent>& dispatched_events() const {
    return dispatched_events_;
  }

  void ClearEvents() { dispatched_events_.clear(); }

 private:
  std::vector<DispatchedEvent> dispatched_events_;
};

class StubStreamProviderDelegate : public StreamProviderDelegate {
 public:
  void DidUpdateStreamProviderState(StreamProvider& stream_provider,
                                    StreamProvider::StreamState old_state,
                                    StreamErrorReason reason) override {}
};

std::unique_ptr<KeyedService> BuildRecordingEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<RecordingEventRouter>(context);
}

std::unique_ptr<KeyedService> BuildDictationKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<DictationKeyedService>(
      Profile::FromBrowserContext(context));
}

scoped_refptr<const extensions::Extension> BuildDictationExtension(
    const std::string& id,
    extensions::mojom::ManifestLocation location) {
  return extensions::ExtensionBuilder("Dictation Test " + id)
      .SetID(id)
      .SetLocation(location)
      .AddAPIPermission("dictationPrivate")
      .Build();
}

scoped_refptr<const extensions::Extension> BuildOtherExtension(
    const std::string& id,
    extensions::mojom::ManifestLocation location) {
  return extensions::ExtensionBuilder("Other Extension " + id)
      .SetID(id)
      .SetLocation(location)
      .Build();
}

}  // namespace

class ListenerStreamProviderTest : public testing::Test {
 public:
  ListenerStreamProviderTest()
      : scoped_feature_list_(CreateEnablingFeatureList()) {
    extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildRecordingEventRouter));
    DictationKeyedServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildDictationKeyedService));
  }
  ~ListenerStreamProviderTest() override = default;

 protected:
  RecordingEventRouter* event_router() {
    return static_cast<RecordingEventRouter*>(
        extensions::EventRouter::Get(&profile_));
  }

  extensions::ExtensionRegistry* registry() {
    return extensions::ExtensionRegistry::Get(&profile_);
  }

  void StartAndStopStream() {
    ListenerStreamProvider provider(&profile_, delegate_);
    provider.BindToTargetAndConnect(std::make_unique<Target>(EmptyTarget()));
    provider.Stop(DictationStreamEndTrigger::kTest);
  }

  void ExpectStartAndEndDispatchedTo(
      const extensions::ExtensionId& extension_id) {
    EXPECT_THAT(
        event_router()->dispatched_events(),
        testing::ElementsAre(
            DispatchedEvent{
                extension_id,
                extensions::api::dictation_private::OnStartStream::kEventName},
            DispatchedEvent{
                extension_id,
                extensions::api::dictation_private::OnEndStream::kEventName}));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  StubStreamProviderDelegate delegate_;
};

TEST_F(ListenerStreamProviderTest, DispatchesToComponentExtensionByDefault) {
  StartAndStopStream();

  ExpectStartAndEndDispatchedTo(extension_misc::kDictationConnectorExtensionId);
}

TEST_F(ListenerStreamProviderTest,
       IgnoresExtensionsWithoutDictationPrivateAccess) {
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kTestExtensionIdA);
  registry()->AddEnabled(
      BuildDictationExtension(extension_misc::kDictationConnectorExtensionId,
                              extensions::mojom::ManifestLocation::kComponent));
  registry()->AddEnabled(BuildOtherExtension(
      kTestExtensionIdA, extensions::mojom::ManifestLocation::kUnpacked));

  StartAndStopStream();

  ExpectStartAndEndDispatchedTo(extension_misc::kDictationConnectorExtensionId);
}

TEST_F(ListenerStreamProviderTest, IgnoresDisabledExtension) {
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kTestExtensionIdA);
  registry()->AddEnabled(
      BuildDictationExtension(extension_misc::kDictationConnectorExtensionId,
                              extensions::mojom::ManifestLocation::kComponent));
  registry()->AddDisabled(BuildDictationExtension(
      kTestExtensionIdA, extensions::mojom::ManifestLocation::kUnpacked));

  StartAndStopStream();

  ExpectStartAndEndDispatchedTo(extension_misc::kDictationConnectorExtensionId);
}

TEST_F(ListenerStreamProviderTest,
       PrefersEnabledNonComponentExtensionOverComponentExtension) {
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kTestExtensionIdA);
  registry()->AddEnabled(
      BuildDictationExtension(extension_misc::kDictationConnectorExtensionId,
                              extensions::mojom::ManifestLocation::kComponent));
  auto unpacked = BuildDictationExtension(
      kTestExtensionIdA, extensions::mojom::ManifestLocation::kUnpacked);
  registry()->AddEnabled(unpacked);

  StartAndStopStream();
  ExpectStartAndEndDispatchedTo(kTestExtensionIdA);

  // Disabling the unpacked extension falls back to the component extension.
  event_router()->ClearEvents();
  registry()->RemoveEnabled(kTestExtensionIdA);
  registry()->AddDisabled(unpacked);

  StartAndStopStream();
  ExpectStartAndEndDispatchedTo(extension_misc::kDictationConnectorExtensionId);
}

}  // namespace dictation
