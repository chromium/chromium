// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/event_initiator_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_actor_attribution_metrics.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_actor_tracker.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/extensions/test_standalone_window_controller.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/glic_enums.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/download/public/common/download_source.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)
#include "chrome/browser/enterprise/connectors/reporting/tab_cdp_actor_tracker.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#endif

namespace enterprise_connectors {

namespace {

using EventInitiatorInfo = chrome::cros::reporting::proto::EventInitiatorInfo;
using ExtensionInfo =
    safe_browsing::ExtensionTelemetryReportRequest::ExtensionInfo;
using chrome::cros::reporting::proto::BrowserAgentInfo;
using extensions::mojom::ManifestLocation;
#if BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)
using chrome::cros::reporting::proto::BrowserFeatureInfo;
using chrome::cros::reporting::proto::ExternalProcessInfo;
#endif

constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kUninstalledExtensionId[] = "ponmlkjihgfedcbaponmlkjihgfedcba";
constexpr char kExtensionName[] = "Test Extension";
constexpr char kExtensionVersion[] = "1.2.3";
constexpr char kConversationId[] = "test-conversation-id";
constexpr char kTriggeringContextId[] = "test-triggering-context-id";

// A download item whose request initiator and download source can be set.
// `content::FakeDownloadItem` supports neither.
class TestDownloadItem : public testing::NiceMock<download::MockDownloadItem> {
 public:
  explicit TestDownloadItem(
      std::optional<url::Origin> request_initiator = std::nullopt,
      download::DownloadSource source = download::DownloadSource::NAVIGATION)
      : request_initiator_(std::move(request_initiator)) {
    ON_CALL(*this, GetRequestInitiator())
        .WillByDefault(testing::ReturnRef(request_initiator_));
    ON_CALL(*this, GetDownloadSource()).WillByDefault(testing::Return(source));
  }

 private:
  std::optional<url::Origin> request_initiator_;
};

}  // namespace

class EventInitiatorUtilsTest : public testing::Test {
 public:
  EventInitiatorUtilsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnterpriseExtensionActorAttribution);
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));
    test::SetOnSecurityEventReporting(profile_.GetPrefs(), /*enabled=*/true);
  }

  ~EventInitiatorUtilsTest() override {
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
  }

  // Creates an extension without registering it in `profile_`.
  scoped_refptr<const extensions::Extension> CreateExtension(
      ManifestLocation location,
      int creation_flags) {
    return extensions::ExtensionBuilder(kExtensionName)
        .SetVersion(kExtensionVersion)
        .SetLocation(location)
        .AddFlags(creation_flags)
        .SetID(kExtensionId)
        .Build();
  }

  // Creates an extension and registers it as enabled in `profile_`.
  scoped_refptr<const extensions::Extension> InstallExtension(
      ManifestLocation location,
      int creation_flags) {
    scoped_refptr<const extensions::Extension> extension =
        CreateExtension(location, creation_flags);
    extensions::ExtensionRegistry::Get(&profile_)->AddEnabled(extension);
    return extension;
  }

  // Creates a download with `request_initiator` and `source` that was not
  // started via the `chrome.downloads` API. If `web_contents` is non-null it is
  // attached as the originating tab, along with `profile_`.
  std::unique_ptr<TestDownloadItem> CreateDownload(
      std::optional<url::Origin> request_initiator = std::nullopt,
      content::WebContents* web_contents = nullptr,
      download::DownloadSource source = download::DownloadSource::NAVIGATION) {
    auto item = std::make_unique<TestDownloadItem>(std::move(request_initiator),
                                                   source);
    content::DownloadItemUtils::AttachInfoForTesting(item.get(), &profile_,
                                                     web_contents);
    return item;
  }

  // Creates a download initiated by extension `id`. If `attach_profile` is
  // true, `profile_` is attached to the item as its BrowserContext, as
  // DownloadManagerImpl does for real downloads.
  std::unique_ptr<TestDownloadItem> CreateDownloadByExtension(
      const std::string& id,
      const std::string& name = kExtensionName,
      bool attach_profile = true) {
    auto item = std::make_unique<TestDownloadItem>();
    if (attach_profile) {
      content::DownloadItemUtils::AttachInfoForTesting(
          item.get(), &profile_, /*web_contents=*/nullptr);
    }
    new extensions::DownloadedByExtension(item.get(), id, name);
    return item;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(EventInitiatorUtilsTest, FeatureDisabledReturnsNullopt) {
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  auto item = CreateDownloadByExtension(kExtensionId);
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Page.navigate");

  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(kEnterpriseExtensionActorAttribution);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
  EXPECT_FALSE(GetActiveExtensionInitiatorInfo(web_contents).has_value());

  // Nothing is recorded while the feature is disabled.
  histogram_tester.ExpectTotalCount(
      kExtensionActorLookupDownloadResultHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kExtensionActorLookupWebContentsResultHistogram, 0);
}

TEST_F(EventInitiatorUtilsTest, NullDownloadReturnsNullopt) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(GetExtensionInitiatorInfo(/*download=*/nullptr).has_value());
  histogram_tester.ExpectTotalCount(
      kExtensionActorLookupDownloadResultHistogram, 0);
}

TEST_F(EventInitiatorUtilsTest, DownloadWithoutExtensionReturnsNullopt) {
  TestDownloadItem item;
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(GetExtensionInitiatorInfo(&item).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kNone, 1);
}

TEST_F(EventInitiatorUtilsTest, EmptyExtensionIdReturnsNullopt) {
  base::HistogramTester histogram_tester;
  auto item = CreateDownloadByExtension(/*id=*/"");
  EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kNone, 1);
}

// A download that extension content requested without the `chrome.downloads`
// API is attributed through its request initiator.
TEST_F(EventInitiatorUtilsTest, ExtensionRequestInitiatorPopulatesProvenance) {
  base::HistogramTester histogram_tester;
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  auto item = CreateDownload(
      extensions::Extension::CreateOriginFromExtensionId(kExtensionId));

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());
  EXPECT_EQ(kExtensionVersion, extension_info->version());
  EXPECT_EQ(ExtensionInfo::INTERNAL, extension_info->install_location());
  EXPECT_TRUE(extension_info->is_from_store());

  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kExtensionRequestInitiator, 1);
}

// Unlike `DownloadedByExtension`, the request initiator carries no name, so an
// extension that is no longer installed is attributed by its ID alone.
TEST_F(EventInitiatorUtilsTest,
       UninstalledExtensionRequestInitiatorReportsIdOnly) {
  auto item = CreateDownload(extensions::Extension::CreateOriginFromExtensionId(
      kUninstalledExtensionId));

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kUninstalledExtensionId, extension_info->id());
  EXPECT_FALSE(extension_info->has_name());
  EXPECT_FALSE(extension_info->has_version());
  EXPECT_FALSE(extension_info->has_install_location());
  EXPECT_FALSE(extension_info->has_is_from_store());
}

// An opaque origin, even one derived from an extension's origin, does not
// identify an extension.
TEST_F(EventInitiatorUtilsTest, OpaqueRequestInitiatorIsNotAttributed) {
  base::HistogramTester histogram_tester;
  auto item = CreateDownload(
      extensions::Extension::CreateOriginFromExtensionId(kExtensionId)
          .DeriveNewOpaqueOrigin());
  EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kNone, 1);
}

// The built-in PDF Viewer navigates the links a user clicks in a PDF, so it is
// the request initiator of downloads the user started.
TEST_F(EventInitiatorUtilsTest, PdfViewerRequestInitiatorIsNotAttributed) {
  base::HistogramTester histogram_tester;
  auto item = CreateDownload(extensions::Extension::CreateOriginFromExtensionId(
      extension_misc::kPdfExtensionId));
  EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kNone, 1);
}

struct DownloadSourceTestCase {
  download::DownloadSource source;
  bool is_attributed;
};

class EventInitiatorUtilsDownloadSourceTest
    : public EventInitiatorUtilsTest,
      public testing::WithParamInterface<DownloadSourceTestCase> {};

// The request initiator only says who started a download when content started
// it. Browser UI reuses the initiator of an earlier request.
TEST_P(EventInitiatorUtilsDownloadSourceTest,
       ExtensionRequestInitiatorAttributesContentDownloadsOnly) {
  base::HistogramTester histogram_tester;
  auto item = CreateDownload(
      extensions::Extension::CreateOriginFromExtensionId(kExtensionId),
      /*web_contents=*/nullptr, GetParam().source);

  EXPECT_EQ(GetParam().is_attributed,
            GetExtensionInitiatorInfo(item.get()).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      GetParam().is_attributed
          ? ExtensionDownloadAttributionSignal::kExtensionRequestInitiator
          : ExtensionDownloadAttributionSignal::kNone,
      1);
}

// Covers every `DownloadSource`. `chrome.downloads` downloads (`EXTENSION_API`)
// are attributed through `DownloadedByExtension` instead.
INSTANTIATE_TEST_SUITE_P(
    All,
    EventInitiatorUtilsDownloadSourceTest,
    testing::Values(
        DownloadSourceTestCase{download::DownloadSource::UNKNOWN, false},
        DownloadSourceTestCase{download::DownloadSource::NAVIGATION, true},
        DownloadSourceTestCase{download::DownloadSource::DRAG_AND_DROP, false},
        DownloadSourceTestCase{download::DownloadSource::FROM_RENDERER, true},
        DownloadSourceTestCase{download::DownloadSource::EXTENSION_API, false},
        DownloadSourceTestCase{download::DownloadSource::EXTENSION_INSTALLER,
                               false},
        DownloadSourceTestCase{download::DownloadSource::INTERNAL_API, false},
        DownloadSourceTestCase{download::DownloadSource::WEB_CONTENTS_API,
                               false},
        DownloadSourceTestCase{download::DownloadSource::OFFLINE_PAGE, false},
        DownloadSourceTestCase{download::DownloadSource::CONTEXT_MENU, false},
        DownloadSourceTestCase{download::DownloadSource::RETRY, false},
        DownloadSourceTestCase{download::DownloadSource::RETRY_FROM_BUBBLE,
                               false},
        DownloadSourceTestCase{download::DownloadSource::TOOLBAR_MENU, false}));

// A reload or back/forward navigation keeps the initiator of the navigation it
// repeats, so the request initiator does not say who started the download.
TEST_F(EventInitiatorUtilsTest,
       ReloadAndHistoryNavigationDownloadsAreNotAttributed) {
  const ui::PageTransition kBackForward = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FORWARD_BACK);
  const std::pair<const char*, ui::PageTransition> kTransitions[] = {
      {"reload", ui::PAGE_TRANSITION_RELOAD},
      {"back/forward", kBackForward},
  };
  for (const auto& [name, transition] : kTransitions) {
    SCOPED_TRACE(name);
    base::HistogramTester histogram_tester;
    auto item = CreateDownload(
        extensions::Extension::CreateOriginFromExtensionId(kExtensionId));
    ON_CALL(*item, GetTransitionType())
        .WillByDefault(testing::Return(transition));

    EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
    histogram_tester.ExpectUniqueSample(
        kExtensionActorLookupDownloadResultHistogram,
        ExtensionDownloadAttributionSignal::kNone, 1);
  }
}

// A web-origin initiator is not an extension signal on its own.
TEST_F(EventInitiatorUtilsTest, WebRequestInitiatorIsNotAnExtensionSignal) {
  base::HistogramTester histogram_tester;
  auto item = CreateDownload(url::Origin::Create(GURL("https://example.com")));
  EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kNone, 1);
}

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
// A download from a tab with an active extension actor is not attributed yet,
// but the signal is recorded.
TEST_F(EventInitiatorUtilsTest, TabActorIsRecordedNotAttributed) {
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Runtime.evaluate");

  base::HistogramTester histogram_tester;
  auto item = CreateDownload(url::Origin::Create(GURL("https://example.com")),
                             web_contents);
  EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kTabActor, 1);
}

// The tab actor is still found when the download has no frame id, e.g. after
// the primary page changed.
TEST_F(EventInitiatorUtilsTest, TabActorIsRecordedWithoutFrameId) {
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Runtime.evaluate");

  base::HistogramTester histogram_tester;
  TestDownloadItem item(url::Origin::Create(GURL("https://example.com")));
  content::DownloadItemUtils::AttachInfo(&item, &profile_, web_contents,
                                         content::GlobalRenderFrameHostId());
  EXPECT_FALSE(GetExtensionInitiatorInfo(&item).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kTabActor, 1);
}

TEST_F(EventInitiatorUtilsTest, TabWithoutActorIsNotAnExtensionSignal) {
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);

  base::HistogramTester histogram_tester;
  auto item = CreateDownload(/*request_initiator=*/std::nullopt, web_contents);
  EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kNone, 1);
}

// Signals are checked in order: chrome.downloads, then request initiator,
// then tab actor. Only the strongest is recorded, and it decides which
// extension is attributed.
TEST_F(EventInitiatorUtilsTest, StrongestDownloadSignalWins) {
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Runtime.evaluate");
  // The request initiator names a second extension, to tell the signals apart.
  const url::Origin initiator_origin =
      extensions::Extension::CreateOriginFromExtensionId(
          kUninstalledExtensionId);

  {
    base::HistogramTester histogram_tester;
    auto item = CreateDownload(initiator_origin, web_contents);
    std::optional<ExtensionInfo> extension_info =
        GetExtensionInitiatorInfo(item.get());
    ASSERT_TRUE(extension_info.has_value());
    EXPECT_EQ(kUninstalledExtensionId, extension_info->id());
    histogram_tester.ExpectUniqueSample(
        kExtensionActorLookupDownloadResultHistogram,
        ExtensionDownloadAttributionSignal::kExtensionRequestInitiator, 1);
  }
  {
    base::HistogramTester histogram_tester;
    auto item = CreateDownload(initiator_origin, web_contents);
    new extensions::DownloadedByExtension(item.get(), kExtensionId,
                                          kExtensionName);
    std::optional<ExtensionInfo> extension_info =
        GetExtensionInitiatorInfo(item.get());
    ASSERT_TRUE(extension_info.has_value());
    EXPECT_EQ(kExtensionId, extension_info->id());
    histogram_tester.ExpectUniqueSample(
        kExtensionActorLookupDownloadResultHistogram,
        ExtensionDownloadAttributionSignal::kDownloadedByExtension, 1);
  }
  {
    // A download started from browser UI is not attributed by its request
    // initiator, which leaves the tab actor as the strongest signal.
    base::HistogramTester histogram_tester;
    auto item = CreateDownload(initiator_origin, web_contents,
                               download::DownloadSource::CONTEXT_MENU);
    EXPECT_FALSE(GetExtensionInitiatorInfo(item.get()).has_value());
    histogram_tester.ExpectUniqueSample(
        kExtensionActorLookupDownloadResultHistogram,
        ExtensionDownloadAttributionSignal::kTabActor, 1);
  }
}
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

TEST_F(EventInitiatorUtilsTest, WebstoreExtensionPopulatesProvenance) {
  base::HistogramTester histogram_tester;
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());
  EXPECT_EQ(kExtensionVersion, extension_info->version());
  EXPECT_EQ(ExtensionInfo::INTERNAL, extension_info->install_location());
  EXPECT_TRUE(extension_info->is_from_store());

  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kDownloadedByExtension, 1);
}

// The name in ExtensionRegistry may differ from the name snapshotted when the
// download started (e.g. after an update); the registry name wins.
TEST_F(EventInitiatorUtilsTest, PrefersRegistryNameOverSnapshottedName) {
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  auto item = CreateDownloadByExtension(kExtensionId, "Stale Name");

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionName, extension_info->name());
}

// A self-hosted, policy-installed extension: pushed by the admin, but not from
// the Web Store. (A policy-installed extension whose update URL is the Web
// Store would be created with FROM_WEBSTORE and report `is_from_store` true.)
TEST_F(EventInitiatorUtilsTest, PolicyInstalledExtensionReportsPolicyLocation) {
  InstallExtension(ManifestLocation::kExternalPolicyDownload,
                   extensions::Extension::NO_FLAGS);
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(ExtensionInfo::EXTERNAL_POLICY_DOWNLOAD,
            extension_info->install_location());
  ASSERT_TRUE(extension_info->has_is_from_store());
  EXPECT_FALSE(extension_info->is_from_store());
}

// A sideloaded extension can claim a Web Store ID, but not its provenance.
TEST_F(EventInitiatorUtilsTest, UnpackedExtensionIsNotMarkedAsFromStore) {
  InstallExtension(ManifestLocation::kUnpacked,
                   extensions::Extension::NO_FLAGS);
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(ExtensionInfo::UNPACKED, extension_info->install_location());
  ASSERT_TRUE(extension_info->has_is_from_store());
  EXPECT_FALSE(extension_info->is_from_store());
}

// An extension may be disabled between starting an action and the event being
// emitted. It must still be fully attributed.
TEST_F(EventInitiatorUtilsTest, DisabledExtensionIsStillAttributed) {
  extensions::ExtensionRegistry::Get(&profile_)->AddDisabled(CreateExtension(
      ManifestLocation::kInternal, extensions::Extension::FROM_WEBSTORE));
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());
  EXPECT_EQ(kExtensionVersion, extension_info->version());
  EXPECT_EQ(ExtensionInfo::INTERNAL, extension_info->install_location());
  EXPECT_TRUE(extension_info->is_from_store());
}

// An extension that is gone by the time the event is emitted is still
// attributed with its ID and snapshotted name, but with no provenance claims
// attached.
TEST_F(EventInitiatorUtilsTest, UnknownExtensionReportsIdWithoutProvenance) {
  base::HistogramTester histogram_tester;
  auto item = CreateDownloadByExtension(kUninstalledExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kUninstalledExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());

  // Provenance stays unset so consumers can tell "unknown" from "false".
  EXPECT_FALSE(extension_info->has_version());
  EXPECT_FALSE(extension_info->has_install_location());
  EXPECT_FALSE(extension_info->has_is_from_store());

  // Still counted as attributed.
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupDownloadResultHistogram,
      ExtensionDownloadAttributionSignal::kDownloadedByExtension, 1);
}

TEST_F(EventInitiatorUtilsTest, EmptySnapshottedNameIsNotSet) {
  auto item = CreateDownloadByExtension(kUninstalledExtensionId, /*name=*/"");

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kUninstalledExtensionId, extension_info->id());
  EXPECT_FALSE(extension_info->has_name());
}

// A download with no BrowserContext attached cannot be looked up in any
// ExtensionRegistry, even if the extension is installed.
TEST_F(EventInitiatorUtilsTest, NoBrowserContextReportsIdAndNameOnly) {
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  auto item = CreateDownloadByExtension(kExtensionId, kExtensionName,
                                        /*attach_profile=*/false);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());
  EXPECT_FALSE(extension_info->has_version());
  EXPECT_FALSE(extension_info->has_install_location());
  EXPECT_FALSE(extension_info->has_is_from_store());
}

struct InstallLocationTestCase {
  ManifestLocation manifest_location;
  ExtensionInfo::InstallLocation expected_install_location;
};

class EventInitiatorUtilsInstallLocationTest
    : public EventInitiatorUtilsTest,
      public testing::WithParamInterface<InstallLocationTestCase> {};

TEST_P(EventInitiatorUtilsInstallLocationTest, MapsManifestLocation) {
  InstallExtension(GetParam().manifest_location,
                   extensions::Extension::NO_FLAGS);
  auto item = CreateDownloadByExtension(kExtensionId);

  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(item.get());

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(GetParam().expected_install_location,
            extension_info->install_location());
}

// Covers every valid ManifestLocation. kInvalidLocation is excluded because an
// installed extension never has it.
INSTANTIATE_TEST_SUITE_P(
    All,
    EventInitiatorUtilsInstallLocationTest,
    testing::Values(
        InstallLocationTestCase{ManifestLocation::kInternal,
                                ExtensionInfo::INTERNAL},
        InstallLocationTestCase{ManifestLocation::kExternalPref,
                                ExtensionInfo::EXTERNAL_PREF},
        InstallLocationTestCase{ManifestLocation::kExternalRegistry,
                                ExtensionInfo::EXTERNAL_REGISTRY},
        InstallLocationTestCase{ManifestLocation::kUnpacked,
                                ExtensionInfo::UNPACKED},
        InstallLocationTestCase{ManifestLocation::kComponent,
                                ExtensionInfo::COMPONENT},
        InstallLocationTestCase{ManifestLocation::kExternalPrefDownload,
                                ExtensionInfo::EXTERNAL_PREF_DOWNLOAD},
        InstallLocationTestCase{ManifestLocation::kExternalPolicyDownload,
                                ExtensionInfo::EXTERNAL_POLICY_DOWNLOAD},
        InstallLocationTestCase{ManifestLocation::kCommandLine,
                                ExtensionInfo::COMMAND_LINE},
        InstallLocationTestCase{ManifestLocation::kExternalPolicy,
                                ExtensionInfo::EXTERNAL_POLICY},
        InstallLocationTestCase{ManifestLocation::kExternalComponent,
                                ExtensionInfo::EXTERNAL_COMPONENT}));

TEST_F(EventInitiatorUtilsTest, NullWebContentsReturnsNullopt) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(
      GetActiveExtensionInitiatorInfo(/*web_contents=*/nullptr).has_value());
  histogram_tester.ExpectTotalCount(
      kExtensionActorLookupWebContentsResultHistogram, 0);
}

TEST_F(EventInitiatorUtilsTest, WebContentsWithoutActiveActorReturnsNullopt) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  EXPECT_FALSE(GetActiveExtensionInitiatorInfo(web_contents).has_value());
  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupWebContentsResultHistogram,
      ExtensionActorAttributionResult::kNoActor, 1);
}

TEST_F(EventInitiatorUtilsTest, WebContentsWithActiveActorPopulatesProvenance) {
  base::HistogramTester histogram_tester;
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Page.navigate");

  std::optional<ExtensionInfo> extension_info =
      GetActiveExtensionInitiatorInfo(web_contents);

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kExtensionId, extension_info->id());
  EXPECT_EQ(kExtensionName, extension_info->name());
  EXPECT_EQ(kExtensionVersion, extension_info->version());
  EXPECT_EQ(ExtensionInfo::INTERNAL, extension_info->install_location());
  EXPECT_TRUE(extension_info->is_from_store());

  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupWebContentsResultHistogram,
      ExtensionActorAttributionResult::kAttributed, 1);
}

TEST_F(EventInitiatorUtilsTest,
       WebContentsWithUninstalledActorReportsIdWithoutProvenance) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ExtensionActorTracker::SetActiveActor(web_contents, kUninstalledExtensionId,
                                        "Page.navigate");

  std::optional<ExtensionInfo> extension_info =
      GetActiveExtensionInitiatorInfo(web_contents);

  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(kUninstalledExtensionId, extension_info->id());
  EXPECT_FALSE(extension_info->has_name());
  EXPECT_FALSE(extension_info->has_version());
  EXPECT_FALSE(extension_info->has_install_location());
  EXPECT_FALSE(extension_info->has_is_from_store());

  histogram_tester.ExpectUniqueSample(
      kExtensionActorLookupWebContentsResultHistogram,
      ExtensionActorAttributionResult::kAttributed, 1);
}

// The `SessionID` overload resolves the tab through the browser's tab strips,
// so only its guard conditions are covered here; resolution of a hosted tab is
// exercised by ExtensionTabUtil's own tests.
TEST_F(EventInitiatorUtilsTest, TabIdWithNullBrowserContextReturnsNullopt) {
  EXPECT_FALSE(GetActiveExtensionInitiatorInfo(/*browser_context=*/nullptr,
                                               SessionID::NewUnique())
                   .has_value());
}

TEST_F(EventInitiatorUtilsTest, InvalidTabIdReturnsNullopt) {
  EXPECT_FALSE(
      GetActiveExtensionInitiatorInfo(&profile_, SessionID::InvalidValue())
          .has_value());
}

TEST_F(EventInitiatorUtilsTest, UnhostedTabIdReturnsNullopt) {
  // A WebContents that is not hosted in any window cannot be found by tab ID,
  // even if it has an active actor.
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  sessions::SessionTabHelper::CreateForWebContents(web_contents,
                                                   base::NullCallback());
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Page.navigate");

  EXPECT_FALSE(
      GetActiveExtensionInitiatorInfo(
          &profile_, sessions::SessionTabHelper::IdForTab(web_contents))
          .has_value());
}

TEST_F(EventInitiatorUtilsTest, TabIdWithFeatureDisabledReturnsNullopt) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(kEnterpriseExtensionActorAttribution);
  EXPECT_FALSE(
      GetActiveExtensionInitiatorInfo(&profile_, SessionID::NewUnique())
          .has_value());
}

TEST_F(EventInitiatorUtilsTest, EventInitiatorNullWebContentsReturnsNullopt) {
  EXPECT_FALSE(
      GetActiveEventInitiatorInfo(/*web_contents=*/nullptr).has_value());
}

TEST_F(EventInitiatorUtilsTest,
       EventInitiatorWebContentsWithoutActiveActorReturnsNullopt) {
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  EXPECT_FALSE(GetActiveEventInitiatorInfo(web_contents).has_value());
}

TEST_F(EventInitiatorUtilsTest,
       EventInitiatorWebContentsPopulatesChromeExtension) {
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Page.navigate");

  std::optional<EventInitiatorInfo> initiator =
      GetActiveEventInitiatorInfo(web_contents);

  ASSERT_TRUE(initiator.has_value());
  ASSERT_TRUE(initiator->has_chrome_extension());
  EXPECT_FALSE(initiator->has_external_process_info());
  EXPECT_FALSE(initiator->has_browser_feature_info());
  EXPECT_EQ(kExtensionId, initiator->chrome_extension().id());
  EXPECT_EQ(kExtensionName, initiator->chrome_extension().name());
  EXPECT_EQ(kExtensionVersion, initiator->chrome_extension().version());
  EXPECT_EQ(ExtensionInfo::INTERNAL,
            initiator->chrome_extension().install_location());
  EXPECT_TRUE(initiator->chrome_extension().is_from_store());
}

TEST_F(EventInitiatorUtilsTest, EventInitiatorTabIdGuardConditions) {
  EXPECT_FALSE(GetActiveEventInitiatorInfo(/*browser_context=*/nullptr,
                                           SessionID::NewUnique())
                   .has_value());
  EXPECT_FALSE(GetActiveEventInitiatorInfo(&profile_, SessionID::InvalidValue())
                   .has_value());

  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  sessions::SessionTabHelper::CreateForWebContents(web_contents,
                                                   base::NullCallback());
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Page.navigate");
  EXPECT_FALSE(
      GetActiveEventInitiatorInfo(
          &profile_, sessions::SessionTabHelper::IdForTab(web_contents))
          .has_value());
}

#if BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)
TEST_F(EventInitiatorUtilsTest,
       EventInitiatorWebContentsFallsBackToCdpExternalProcess) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kEventReportingCdpAttribution},
      /*disabled_features=*/{kEnterpriseExtensionActorAttribution});

  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ProcessTree tree;
  tree.process_nodes.emplace_back(/*process_id=*/4242,
                                  /*parent_process_id=*/100, "/usr/bin/node",
                                  "/tmp/agent.js");
  TabCdpActorTracker::GetOrCreateForWebContents(web_contents)
      ->buffer()
      .OnExternalActorCommandHandled(
          ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
          net::IPEndPoint(net::IPAddress::IPv4Localhost(), 51234),
          std::move(tree));

  std::optional<EventInitiatorInfo> initiator =
      GetActiveEventInitiatorInfo(web_contents);

  ASSERT_TRUE(initiator.has_value());
  EXPECT_FALSE(initiator->has_chrome_extension());
  EXPECT_FALSE(initiator->has_browser_feature_info());
  ASSERT_TRUE(initiator->has_external_process_info());
  ASSERT_EQ(1, initiator->external_process_info().external_processes_size());
  const auto& process =
      initiator->external_process_info().external_processes(0);
  EXPECT_EQ(ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
            process.transport_type());
  ASSERT_EQ(1, process.process_nodes_size());
  EXPECT_EQ(4242, process.process_nodes(0).process_id());
  EXPECT_EQ("/usr/bin/node", process.process_nodes(0).executable_path());
}

TEST_F(EventInitiatorUtilsTest,
       EventInitiatorWebContentsFallsBackToCdpDevToolsUi) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kEventReportingCdpAttribution},
      /*disabled_features=*/{kEnterpriseExtensionActorAttribution});

  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  TabCdpActorTracker::GetOrCreateForWebContents(web_contents)
      ->buffer()
      .OnDevToolsUiAttached();

  std::optional<EventInitiatorInfo> initiator =
      GetActiveEventInitiatorInfo(web_contents);

  ASSERT_TRUE(initiator.has_value());
  EXPECT_FALSE(initiator->has_chrome_extension());
  EXPECT_FALSE(initiator->has_external_process_info());
  ASSERT_TRUE(initiator->has_browser_feature_info());
  EXPECT_EQ(BrowserFeatureInfo::FEATURE_TYPE_DEVTOOLS_UI,
            initiator->browser_feature_info().feature_type());
}

TEST_F(EventInitiatorUtilsTest,
       EventInitiatorExtensionTakesPrecedenceOverCdpActor) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kEnterpriseExtensionActorAttribution,
                            kEventReportingCdpAttribution},
      /*disabled_features=*/{});

  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Page.navigate");
  ProcessTree tree;
  tree.process_nodes.emplace_back(/*process_id=*/4242,
                                  /*parent_process_id=*/100, "/usr/bin/node",
                                  "");
  TabCdpActorTracker::GetOrCreateForWebContents(web_contents)
      ->buffer()
      .OnExternalActorCommandHandled(
          ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
          net::IPEndPoint(net::IPAddress::IPv4Localhost(), 51234),
          std::move(tree));

  std::optional<EventInitiatorInfo> initiator =
      GetActiveEventInitiatorInfo(web_contents);
  ASSERT_TRUE(initiator.has_value());
  ASSERT_TRUE(initiator->has_chrome_extension());
  EXPECT_FALSE(initiator->has_external_process_info());
  EXPECT_EQ(kExtensionId, initiator->chrome_extension().id());
}

TEST_F(EventInitiatorUtilsTest,
       EventInitiatorTabIdGatesCdpOnUrlLookupDeviceInfoEligibility) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kEnterpriseExtensionActorAttribution,
                            kEventReportingCdpAttribution},
      /*disabled_features=*/{});

  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  sessions::SessionTabHelper::CreateForWebContents(web_contents,
                                                   base::NullCallback());
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  extensions::TestStandaloneWindowController window_controller(
      /*base_window=*/nullptr, &profile_, SessionID::NewUnique(), web_contents);

  ProcessTree tree;
  tree.process_nodes.emplace_back(/*process_id=*/4242,
                                  /*parent_process_id=*/100, "/usr/bin/node",
                                  "/tmp/agent.js");
  TabCdpActorTracker::GetOrCreateForWebContents(web_contents)
      ->buffer()
      .OnExternalActorCommandHandled(
          ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
          net::IPEndPoint(net::IPAddress::IPv4Localhost(), 51234),
          std::move(tree));

  // Without real-time URL check enabled, device-level CDP attribution is not
  // returned for the tab ID overload.
  EXPECT_FALSE(GetActiveEventInitiatorInfo(&profile_, tab_id).has_value());

  // User-scoped real-time URL check on an unaffiliated profile does not allow
  // device info, so CDP attribution is suppressed while extension attribution
  // is still returned when an extension actor is active.
  profile_.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckMode,
                                  REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile_.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                  policy::POLICY_SCOPE_USER);
  EXPECT_FALSE(GetActiveEventInitiatorInfo(&profile_, tab_id).has_value());

  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Page.navigate");
  std::optional<EventInitiatorInfo> ext_initiator =
      GetActiveEventInitiatorInfo(&profile_, tab_id);
  ASSERT_TRUE(ext_initiator.has_value());
  EXPECT_TRUE(ext_initiator->has_chrome_extension());
  EXPECT_FALSE(ext_initiator->has_external_process_info());
  EXPECT_EQ(kExtensionId, ext_initiator->chrome_extension().id());

  // Clear the active extension actor and switch real-time URL check to machine
  // scope; device-level CDP attribution is now included.
  web_contents->RemoveUserData(ExtensionActorTracker::UserDataKey());
  profile_.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                  policy::POLICY_SCOPE_MACHINE);
  std::optional<EventInitiatorInfo> cdp_initiator =
      GetActiveEventInitiatorInfo(&profile_, tab_id);
  ASSERT_TRUE(cdp_initiator.has_value());
  EXPECT_FALSE(cdp_initiator->has_chrome_extension());
  ASSERT_TRUE(cdp_initiator->has_external_process_info());
  ASSERT_EQ(1,
            cdp_initiator->external_process_info().external_processes_size());
  EXPECT_EQ(4242, cdp_initiator->external_process_info()
                      .external_processes(0)
                      .process_nodes(0)
                      .process_id());
}

TEST_F(EventInitiatorUtilsTest,
       EventInitiatorWebContentsGatesCdpOnReportingDeviceInfoEligibility) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kEnterpriseExtensionActorAttribution,
                            kEventReportingCdpAttribution},
      /*disabled_features=*/{});

  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  ProcessTree tree;
  tree.process_nodes.emplace_back(/*process_id=*/4242,
                                  /*parent_process_id=*/100, "/usr/bin/node",
                                  "/tmp/agent.js");
  TabCdpActorTracker::GetOrCreateForWebContents(web_contents)
      ->buffer()
      .OnExternalActorCommandHandled(
          ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
          net::IPEndPoint(net::IPAddress::IPv4Localhost(), 51234),
          std::move(tree));

  // Even when machine-scoped real-time URL check is enabled, user-scoped
  // security event reporting on an unaffiliated profile does not allow device
  // info for the `WebContents` overload.
  profile_.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckMode,
                                  REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile_.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                  policy::POLICY_SCOPE_MACHINE);
  test::SetOnSecurityEventReporting(profile_.GetPrefs(), /*enabled=*/true,
                                    /*enabled_event_names=*/{},
                                    /*enabled_opt_in_events=*/{},
                                    /*machine_scope=*/false);
  EXPECT_FALSE(GetActiveEventInitiatorInfo(web_contents).has_value());

  // Extension attribution is profile-scoped and still returned.
  InstallExtension(ManifestLocation::kInternal,
                   extensions::Extension::FROM_WEBSTORE);
  ExtensionActorTracker::SetActiveActor(web_contents, kExtensionId,
                                        "Page.navigate");
  std::optional<EventInitiatorInfo> ext_initiator =
      GetActiveEventInitiatorInfo(web_contents);
  ASSERT_TRUE(ext_initiator.has_value());
  EXPECT_TRUE(ext_initiator->has_chrome_extension());
  EXPECT_FALSE(ext_initiator->has_external_process_info());
  EXPECT_EQ(kExtensionId, ext_initiator->chrome_extension().id());
}
#endif  // BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)

// Tests attribution to Gemini in Chrome (Glic) actor tasks. Members are
// declared so that `policy_checker_` and `mock_tab_` outlive `profile_`, whose
// `ActorKeyedService` refers to both.
class EventInitiatorUtilsGlicActorTest : public testing::Test {
 public:
  EventInitiatorUtilsGlicActorTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kEnterpriseExtensionActorAttribution,
                              kEnterpriseGlicActorAttribution},
        /*disabled_features=*/{});
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));
    test::SetOnSecurityEventReporting(profile_.GetPrefs(), /*enabled=*/true);

    actor::ActorKeyedServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<actor::ActorKeyedServiceFake>(
              Profile::FromBrowserContext(context));
        }));

    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);
    ON_CALL(mock_tab_, GetProfile).WillByDefault(testing::Return(&profile_));
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents_,
                                                         &mock_tab_);
    actor_service()->GetSurfaceRegistry().OnTabCreated(mock_tab_);
  }

  ~EventInitiatorUtilsGlicActorTest() override {
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
  }

  actor::ActorKeyedServiceFake* actor_service() {
    return static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedService::Get(&profile_));
  }

  // Starts a Glic task with the given source ID and feature mode, acting on
  // `web_contents_`.
  actor::TaskId StartGlicTask(std::optional<std::string> source_id,
                              glic::mojom::FeatureMode feature_mode =
                                  glic::mojom::FeatureMode::kActuation) {
    actor::ActorTaskOptions options;
    options.webui_task_options = actor::webui::mojom::TaskOptions::New();
    options.webui_task_options->feature_mode = feature_mode;
    // `ActorKeyedServiceFake` hides the base class overload that takes a task
    // source, so call it through the base class.
    actor::ActorKeyedService* service = actor_service();
    actor::TaskId task_id = service->CreateTaskForTesting(
        actor::ui::NewMockUiEventDispatcher(),
        actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic,
                              std::move(source_id)),
        &policy_checker_, std::move(options));
    AddTabToTask(task_id);
    return task_id;
  }

  // Makes the task `task_id` act on `web_contents_`.
  void AddTabToTask(actor::TaskId task_id) {
    actor_service()->GetTask(task_id)->AddTab(
        mock_tab_.GetHandle(), /*stop_task_on_detach=*/true, base::DoNothing());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  actor::MockPolicyChecker policy_checker_{
      actor::EnterprisePolicyChecker::UrlBlockReason::kNotBlocked};
  tabs::MockTabInterface mock_tab_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

TEST_F(EventInitiatorUtilsGlicActorTest, ActingGlicTaskIsAttributed) {
  actor::TaskId task_id = StartGlicTask(kConversationId);

  std::optional<BrowserAgentInfo> agent_info =
      GetActiveGlicActorInitiatorInfo(web_contents_);
  ASSERT_TRUE(agent_info.has_value());
  EXPECT_EQ(BrowserAgentInfo::AGENT_TYPE_GEMINI_IN_CHROME,
            agent_info->agent_type());
  EXPECT_EQ(base::NumberToString(task_id.value()), agent_info->task_id());
  EXPECT_EQ(kConversationId, agent_info->conversation_id());

  std::optional<EventInitiatorInfo> initiator =
      GetActiveEventInitiatorInfo(web_contents_);
  ASSERT_TRUE(initiator.has_value());
  EXPECT_FALSE(initiator->has_chrome_extension());
  EXPECT_FALSE(initiator->has_external_process_info());
  EXPECT_FALSE(initiator->has_browser_feature_info());
  ASSERT_TRUE(initiator->has_browser_agent_info());
  EXPECT_EQ(BrowserAgentInfo::AGENT_TYPE_GEMINI_IN_CHROME,
            initiator->browser_agent_info().agent_type());
  EXPECT_EQ(base::NumberToString(task_id.value()),
            initiator->browser_agent_info().task_id());
  EXPECT_EQ(kConversationId, initiator->browser_agent_info().conversation_id());
}

TEST_F(EventInitiatorUtilsGlicActorTest, FeatureDisabledReturnsNullopt) {
  StartGlicTask(kConversationId);

  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(kEnterpriseGlicActorAttribution);
  EXPECT_FALSE(GetActiveGlicActorInitiatorInfo(web_contents_).has_value());
  EXPECT_FALSE(GetActiveEventInitiatorInfo(web_contents_).has_value());
}

TEST_F(EventInitiatorUtilsGlicActorTest, NullWebContentsReturnsNullopt) {
  EXPECT_FALSE(
      GetActiveGlicActorInitiatorInfo(/*web_contents=*/nullptr).has_value());
}

TEST_F(EventInitiatorUtilsGlicActorTest, NoActingTaskReturnsNullopt) {
  EXPECT_FALSE(GetActiveGlicActorInitiatorInfo(web_contents_).has_value());
  EXPECT_FALSE(GetActiveEventInitiatorInfo(web_contents_).has_value());
}

TEST_F(EventInitiatorUtilsGlicActorTest, PausedTaskReturnsNullopt) {
  actor::TaskId task_id = StartGlicTask(kConversationId);
  actor_service()->PauseTaskForTesting(task_id, /*from_actor=*/false);

  EXPECT_FALSE(GetActiveGlicActorInitiatorInfo(web_contents_).has_value());
}

TEST_F(EventInitiatorUtilsGlicActorTest, StoppedTaskReturnsNullopt) {
  actor::TaskId task_id = StartGlicTask(kConversationId);
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);

  EXPECT_FALSE(GetActiveGlicActorInitiatorInfo(web_contents_).has_value());
}

TEST_F(EventInitiatorUtilsGlicActorTest, NonGlicTaskReturnsNullopt) {
  // `ActorKeyedServiceFake` creates tasks with the test client as the source.
  AddTabToTask(actor_service()->CreateTaskForTesting());

  EXPECT_FALSE(GetActiveGlicActorInitiatorInfo(web_contents_).has_value());
  EXPECT_FALSE(GetActiveEventInitiatorInfo(web_contents_).has_value());
}

TEST_F(EventInitiatorUtilsGlicActorTest,
       TaskWithoutConversationIdLeavesItUnset) {
  actor::TaskId task_id = StartGlicTask(/*source_id=*/std::nullopt);

  std::optional<BrowserAgentInfo> agent_info =
      GetActiveGlicActorInitiatorInfo(web_contents_);
  ASSERT_TRUE(agent_info.has_value());
  EXPECT_EQ(base::NumberToString(task_id.value()), agent_info->task_id());
  EXPECT_TRUE(agent_info->conversation_id().empty());
}

TEST_F(EventInitiatorUtilsGlicActorTest,
       ExperimentalTriggeringTaskDoesNotReportSourceId) {
  actor::TaskId task_id = StartGlicTask(
      kTriggeringContextId, glic::mojom::FeatureMode::kExperimentalTriggering);

  std::optional<BrowserAgentInfo> agent_info =
      GetActiveGlicActorInitiatorInfo(web_contents_);
  ASSERT_TRUE(agent_info.has_value());
  EXPECT_EQ(BrowserAgentInfo::AGENT_TYPE_GEMINI_IN_CHROME,
            agent_info->agent_type());
  EXPECT_EQ(base::NumberToString(task_id.value()), agent_info->task_id());
  EXPECT_TRUE(agent_info->conversation_id().empty());
}

TEST_F(EventInitiatorUtilsGlicActorTest, OffTheRecordProfileReturnsNullopt) {
  content::WebContents* otr_web_contents =
      web_contents_factory_.CreateWebContents(
          profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true));

  EXPECT_FALSE(GetActiveGlicActorInitiatorInfo(otr_web_contents).has_value());
}

TEST_F(EventInitiatorUtilsGlicActorTest, ExtensionTakesPrecedenceOverGlicTask) {
  StartGlicTask(kConversationId);
  ExtensionActorTracker::SetActiveActor(web_contents_, kExtensionId,
                                        "Page.navigate");

  std::optional<EventInitiatorInfo> initiator =
      GetActiveEventInitiatorInfo(web_contents_);
  ASSERT_TRUE(initiator.has_value());
  ASSERT_TRUE(initiator->has_chrome_extension());
  EXPECT_FALSE(initiator->has_browser_agent_info());
  EXPECT_EQ(kExtensionId, initiator->chrome_extension().id());
}

TEST_F(EventInitiatorUtilsGlicActorTest,
       TabIdIncludesGlicTaskWhenOnlyGlicAttributionIsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kEnterpriseGlicActorAttribution},
      /*disabled_features=*/{kEnterpriseExtensionActorAttribution,
                             kEventReportingCdpAttribution});

  sessions::SessionTabHelper::CreateForWebContents(web_contents_,
                                                   base::NullCallback());
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents_);
  extensions::TestStandaloneWindowController window_controller(
      /*base_window=*/nullptr, &profile_, SessionID::NewUnique(),
      web_contents_);
  actor::TaskId task_id = StartGlicTask(kConversationId);

  std::optional<EventInitiatorInfo> initiator =
      GetActiveEventInitiatorInfo(&profile_, tab_id);
  ASSERT_TRUE(initiator.has_value());
  ASSERT_TRUE(initiator->has_browser_agent_info());
  EXPECT_EQ(base::NumberToString(task_id.value()),
            initiator->browser_agent_info().task_id());
  EXPECT_EQ(kConversationId, initiator->browser_agent_info().conversation_id());
}

#if BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)
TEST_F(EventInitiatorUtilsGlicActorTest, CdpActorTakesPrecedenceOverGlicTask) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kEventReportingCdpAttribution,
                            kEnterpriseGlicActorAttribution},
      /*disabled_features=*/{kEnterpriseExtensionActorAttribution});

  StartGlicTask(kConversationId);
  ProcessTree tree;
  tree.process_nodes.emplace_back(/*process_id=*/4242,
                                  /*parent_process_id=*/100, "/usr/bin/node",
                                  "/tmp/agent.js");
  TabCdpActorTracker::GetOrCreateForWebContents(web_contents_)
      ->buffer()
      .OnExternalActorCommandHandled(
          ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
          net::IPEndPoint(net::IPAddress::IPv4Localhost(), 51234),
          std::move(tree));

  std::optional<EventInitiatorInfo> initiator =
      GetActiveEventInitiatorInfo(web_contents_);
  ASSERT_TRUE(initiator.has_value());
  EXPECT_TRUE(initiator->has_external_process_info());
  EXPECT_FALSE(initiator->has_browser_agent_info());
}
#endif  // BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)

}  // namespace enterprise_connectors
