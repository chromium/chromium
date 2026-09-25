// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/low_trust_policy_install_block_manager.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/test_extension_prefs.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kBlockedLowTrustPolicyExtensions[] =
    "extensions.blocked_low_trust_policy_installs";
constexpr char kOverrideTypeKey[] = "override_type";
constexpr char kUpdateUrlKey[] = "update_url";
constexpr char kTimestampKey[] = "timestamp";

constexpr char kStaleId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kFreshId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr char kMissingTimestampId[] = "cccccccccccccccccccccccccccccccc";
constexpr char kMissingUpdateUrlId[] = "dddddddddddddddddddddddddddddddd";
constexpr char kInvalidEnumId[] = "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
constexpr char kNonDictId[] = "ffffffffffffffffffffffffffffffff";

}  // namespace

class LowTrustPolicyInstallBlockManagerTest : public testing::Test {
 public:
  LowTrustPolicyInstallBlockManagerTest() = default;

  void SetUp() override {
    test_prefs_ = std::make_unique<TestExtensionPrefs>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        std::make_unique<TestingProfile>());
    LowTrustPolicyInstallBlockManager::RegisterProfilePrefs(
        test_prefs_->pref_registry().get());
    ExtensionManagement* extension_management =
        ExtensionManagementFactory::GetForBrowserContext(
            test_prefs_->browser_context());
    manager_ = std::make_unique<LowTrustPolicyInstallBlockManager>(
        test_prefs_->browser_context(), *test_prefs_->prefs()->pref_service(),
        *extension_management);
  }

 protected:
  PrefService* pref_service() { return test_prefs_->prefs()->pref_service(); }
  LowTrustPolicyInstallBlockManager* manager() { return manager_.get(); }

  // Populates the preference store with a representative set of entries for
  // testing: a fresh valid entry, a stale (expired TTL) entry, and various
  // malformed records (missing timestamp, missing update URL, invalid enum
  // value, and non-dictionary value).
  void PopulateTestEntries(base::Time now) {
    base::TimeDelta ttl = LowTrustPolicyInstallBlockManager::GetTTLForTesting();

    manager()->MarkBlocked(
        kStaleId,
        BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kDse,
                             .update_url = "http://example1.com",
                             .timestamp = now - (ttl + base::Days(1))});

    manager()->MarkBlocked(
        kFreshId,
        BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kNtp,
                             .update_url = "http://example2.com",
                             .timestamp = now - (ttl / 2)});

    ScopedDictPrefUpdate update(pref_service(),
                                kBlockedLowTrustPolicyExtensions);

    base::DictValue missing_timestamp_entry;
    missing_timestamp_entry.Set(kOverrideTypeKey, 1);
    missing_timestamp_entry.Set(kUpdateUrlKey, "http://example.com");
    update->Set(kMissingTimestampId, std::move(missing_timestamp_entry));

    base::DictValue missing_update_url_entry;
    missing_update_url_entry.Set(kOverrideTypeKey, 1);
    missing_update_url_entry.Set(kTimestampKey, base::TimeToValue(now));
    update->Set(kMissingUpdateUrlId, std::move(missing_update_url_entry));

    base::DictValue invalid_enum_entry;
    invalid_enum_entry.Set(kOverrideTypeKey, 999);
    invalid_enum_entry.Set(kUpdateUrlKey, "http://example.com");
    invalid_enum_entry.Set(kTimestampKey, base::TimeToValue(now));
    update->Set(kInvalidEnumId, std::move(invalid_enum_entry));

    update->Set(kNonDictId, "invalid_string_value");
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestExtensionPrefs> test_prefs_;
  std::unique_ptr<LowTrustPolicyInstallBlockManager> manager_;
};

// Verifies basic lifecycle operations: marking an extension as blocked,
// querying blocked state, updating metadata, and clearing records.
TEST_F(LowTrustPolicyInstallBlockManagerTest, BlockedByLowTrust) {
  constexpr char kTestId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  constexpr char kTestId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  constexpr char kNonExistentId[] = "cccccccccccccccccccccccccccccccc";
  base::Time now = base::Time::Now();

  manager()->MarkBlocked(
      kTestId1,
      BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kDse,
                           .update_url = "http://example1.com",
                           .timestamp = now});

  EXPECT_TRUE(manager()->IsBlocked(kTestId1));
  EXPECT_FALSE(manager()->IsBlocked(kTestId2));

  auto map = manager()->GetAllBlocked();
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map[kTestId1].override_type, util::DseNtpOverrideType::kDse);
  EXPECT_EQ(map[kTestId1].update_url, "http://example1.com");
  EXPECT_EQ(map[kTestId1].timestamp, now);

  // Clearing a non-existent ID should be a safe no-op.
  manager()->Clear(kNonExistentId);
  EXPECT_FALSE(manager()->IsBlocked(kNonExistentId));

  manager()->Clear(kTestId1);
  EXPECT_FALSE(manager()->IsBlocked(kTestId1));
  EXPECT_TRUE(manager()->GetAllBlocked().empty());

  manager()->MarkBlocked(
      kTestId1,
      BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kNtp,
                           .update_url = "http://example2.com",
                           .timestamp = now});
  EXPECT_TRUE(manager()->IsBlocked(kTestId1));
  map = manager()->GetAllBlocked();
  EXPECT_EQ(map[kTestId1].override_type, util::DseNtpOverrideType::kNtp);
  EXPECT_EQ(map[kTestId1].update_url, "http://example2.com");
}

// Verifies that query methods filter out stale records (expired TTLs) and
// malformed preference entries (missing timestamps, missing update URLs,
// invalid enum values, or non-dictionary structures) while preserving valid,
// active entries.
TEST_F(LowTrustPolicyInstallBlockManagerTest,
       QueryFiltersStaleAndMalformedEntries) {
  base::Time now = base::Time::Now();
  PopulateTestEntries(now);

  EXPECT_FALSE(manager()->IsBlocked(kStaleId));
  EXPECT_FALSE(manager()->IsBlocked(kMissingTimestampId));
  EXPECT_FALSE(manager()->IsBlocked(kMissingUpdateUrlId));
  EXPECT_FALSE(manager()->IsBlocked(kInvalidEnumId));
  EXPECT_FALSE(manager()->IsBlocked(kNonDictId));
  EXPECT_TRUE(manager()->IsBlocked(kFreshId));

  auto map = manager()->GetAllBlocked();
  EXPECT_EQ(map.size(), 1u);
  EXPECT_TRUE(map.contains(kFreshId));
  EXPECT_FALSE(map.contains(kStaleId));
  EXPECT_FALSE(map.contains(kMissingTimestampId));
  EXPECT_FALSE(map.contains(kMissingUpdateUrlId));
  EXPECT_FALSE(map.contains(kInvalidEnumId));
  EXPECT_FALSE(map.contains(kNonDictId));
}

// Verifies that CleanupStaleRecords purges all categories of stale and
// malformed records (expired TTLs, missing fields, invalid enum values, and
// non-dictionary structures) while retaining valid entries.
TEST_F(LowTrustPolicyInstallBlockManagerTest, CleanupStaleRecords) {
  base::Time now = base::Time::Now();
  PopulateTestEntries(now);

  // All 5 stale and malformed records should be purged in a single pass.
  EXPECT_EQ(manager()->CleanupStaleRecords(), 5u);
  EXPECT_EQ(manager()->CleanupStaleRecords(), 0u);

  EXPECT_TRUE(manager()->IsBlocked(kFreshId));
}

class LowTrustPolicyInstallBlockManagerServiceTest
    : public ExtensionServiceTestBase {
 protected:
  static constexpr char kRetainedId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  static constexpr char kRemovedId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  static constexpr char kUpdateUrl[] =
      "https://clients2.google.com/service/update2/crx";

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    policy_provider()->SetDefaultReturns(
        /*is_initialization_complete_return=*/false,
        /*is_first_policy_load_complete_return=*/false);
    ExtensionServiceInitParams params;
    params.prefs_content = "{}";
    params.autoupdate_enabled = false;
    InitializeExtensionService(std::move(params));
    block_manager()->SetPolicyServiceForTesting(policy_service());
  }

  LowTrustPolicyInstallBlockManager* block_manager() {
    return ExtensionManagementFactory::GetForBrowserContext(profile())
        ->low_trust_block_manager();
  }

  void SeedBlockedCacheEntries() {
    block_manager()->MarkBlocked(
        kRetainedId,
        BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kDse,
                             .update_url = kUpdateUrl,
                             .timestamp = base::Time::Now()});
    block_manager()->MarkBlocked(
        kRemovedId,
        BlockedExtensionInfo{.override_type = util::DseNtpOverrideType::kNtp,
                             .update_url = kUpdateUrl,
                             .timestamp = base::Time::Now()});
    ASSERT_TRUE(block_manager()->IsBlocked(kRetainedId));
    ASSERT_TRUE(block_manager()->IsBlocked(kRemovedId));
  }

  void SetForceInstallPolicy(const ExtensionId& extension_id,
                             const std::string& update_url) {
    policy_provider()->SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::PolicyMap policies;
    base::ListValue forcelist;
    forcelist.Append(extension_id + ";" + update_url);
    policies.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_PLATFORM,
                 base::Value(std::move(forcelist)),
                 /*external_data_fetcher=*/nullptr);
    policy_provider()->UpdateChromePolicy(policies);
  }
};

// Verifies that when an enterprise policy is updated at runtime to remove a
// previously blocked extension, LowTrustPolicyInstallBlockManager observes the
// ExtensionManagement update and evicts the removed extension from the blocked
// cache while retaining extensions that remain configured in policy.
TEST_F(LowTrustPolicyInstallBlockManagerServiceTest,
       LowTrustPolicyRemovalCleanup) {
  SeedBlockedCacheEntries();

  // Configure force-install policy for kRetainedId only (simulating removal of
  // kRemovedId). Updating the managed pref triggers
  // ExtensionManagement::Refresh(), which notifies
  // LowTrustPolicyInstallBlockManager::OnExtensionManagementSettingsChanged().
  SetForceInstallPolicy(kRetainedId, kUpdateUrl);

  EXPECT_TRUE(block_manager()->IsBlocked(kRetainedId));
  EXPECT_FALSE(block_manager()->IsBlocked(kRemovedId));
}

// Verifies that when the browser starts up with persisted entries in the
// low-trust blocked cache from a previous session and policies are already
// loaded, LowTrustPolicyInstallBlockManager evicts entries whose enterprise
// policies are no longer configured once ExtensionSystem::ready() is signaled.
TEST_F(LowTrustPolicyInstallBlockManagerServiceTest,
       LowTrustPolicyRemovalStartupCleanup) {
  // Simulate a profile starting up where PolicyService has already loaded
  // kRetainedId, while the persisted blocked cache still holds both kRetainedId
  // and kRemovedId prior to ExtensionService::Init() signaling
  // ExtensionSystem::ready().
  SetForceInstallPolicy(kRetainedId, kUpdateUrl);
  SeedBlockedCacheEntries();
  ASSERT_FALSE(ExtensionSystem::Get(profile())->ready().is_signaled());

  // Initialize ExtensionService so ExtensionSystem::ready() fires and runs the
  // deferred startup cleanup callback queued by `block_manager()`.
  service()->Init();
  base::RunLoop run_loop;
  ExtensionSystem::Get(profile())->ready().Post(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(block_manager()->IsBlocked(kRetainedId));
  EXPECT_FALSE(block_manager()->IsBlocked(kRemovedId));
}

// Verifies that if ExtensionSystem::ready() is signaled before PolicyService
// finishes initializing POLICY_DOMAIN_CHROME, LowTrustPolicyInstallBlockManager
// does not prematurely evict blocked cache entries and instead waits until
// OnPolicyServiceInitialized() fires.
TEST_F(LowTrustPolicyInstallBlockManagerServiceTest,
       LowTrustPolicyRemovalDelayedPolicyInitCleanup) {
  SeedBlockedCacheEntries();
  ASSERT_FALSE(ExtensionSystem::Get(profile())->ready().is_signaled());

  // Initialize ExtensionService so ExtensionSystem::ready() fires while
  // PolicyService is still uninitialized. Neither entry should be prematurely
  // evicted before Chrome-domain policies finish loading.
  service()->Init();
  base::RunLoop run_loop;
  ExtensionSystem::Get(profile())->ready().Post(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(block_manager()->IsBlocked(kRetainedId));
  EXPECT_TRUE(block_manager()->IsBlocked(kRemovedId));

  // Complete PolicyService initialization with only kRetainedId configured in
  // policy. OnPolicyServiceInitialized() should evict kRemovedId while
  // retaining kRetainedId.
  SetForceInstallPolicy(kRetainedId, kUpdateUrl);

  EXPECT_TRUE(block_manager()->IsBlocked(kRetainedId));
  EXPECT_FALSE(block_manager()->IsBlocked(kRemovedId));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Tests that when a policy-installed extension overriding the New Tab Page is
// present on a managed machine, transitioning to an unmanaged (low trust)
// environment uninstalls the extension and marks it in the low-trust blocked
// manager to restore user control and protect from persistent overrides.
TEST_F(LowTrustPolicyInstallBlockManagerServiceTest,
       LowTrustTransitionUninstall) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBlockPolicyDseNtpOverridesInLowTrust);

  // 1. In a managed environment, policy installations of settings-override
  // extensions are trusted and permitted to install and run normally.
  policy::ScopedManagementServiceOverrideForTesting trusted_profile_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  auto extension =
      ExtensionBuilder("Policy NTP Override")
          .SetLocation(mojom::ManifestLocation::kExternalPolicyDownload)
          .AddJSON(R"(
            "chrome_url_overrides": {
              "newtab": "custom_newtab.html"
            }
          )")
          .Build();

  SetForceInstallPolicy(extension->id(), kUpdateUrl);
  registrar()->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(),
                                    kInstallFlagInstallImmediately);

  // Policy extensions are enabled upon install in a trusted environment.
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // 2. Simulate transition to an unmanaged (low trust) environment where
  // policy keys remain present on disk.
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::NONE);

  // Trigger low-trust uninstallation of active policy-installed settings
  // override extensions via the ExtensionManagement::Observer callback.
  block_manager()->OnExtensionManagementSettingsChanged();

  // When management trust is lost, active policy-installed settings-override
  // extensions must be uninstalled to restore user control, and their IDs must
  // be cached in the low-trust blocked manager to prevent subsequent installs.
  EXPECT_FALSE(registry()->GetInstalledExtension(extension->id()));
  EXPECT_TRUE(block_manager()->IsBlocked(extension->id()));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace extensions
