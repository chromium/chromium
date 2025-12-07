// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/dns/dns_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "extensions/test/result_catcher.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_dns_util.h"
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {
using extensions::api_test_utils::RunFunctionAndReturnSingleResult;

constexpr char kHostname[] = "www.sowbug.test";
constexpr char kAddress[] = "9.8.7.6";

// Returns //extensions/test/data.
base::FilePath GetExtensionsDirTestData() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_root_path;
  base::PathService::Get(extensions::DIR_TEST_DATA, &test_root_path);
  return test_root_path;
}

}  // namespace

class DnsApiTest : public ExtensionApiTest {
 public:
  DnsApiTest() {
    // Enable kPartitionConnectionsByNetworkIsolationKey so the test can verify
    // that the correct NetworkAnonymizationKey was used for the DNS lookup.
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
  }

 private:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule(kHostname, kAddress);
    host_resolver()->AddSimulatedFailure("this.hostname.is.bogus.test");
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsResolveIPLiteral) {
  scoped_refptr<DnsResolveFunction> resolve_function(new DnsResolveFunction());
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  resolve_function->set_extension(empty_extension.get());
  resolve_function->set_has_callback(true);

  std::optional<base::Value> result(RunFunctionAndReturnSingleResult(
      resolve_function.get(), "[\"127.0.0.1\"]", profile()));
  const base::Value::Dict& dict = result->GetDict();

  EXPECT_EQ(net::OK, dict.FindInt("resultCode"));

  const std::string* address = dict.FindString("address");
  ASSERT_TRUE(address);
  EXPECT_EQ("127.0.0.1", *address);
}

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsResolveHostname) {
  ResultCatcher catcher;
  // Load a simple test extension.
  const Extension* extension =
      LoadExtension(GetExtensionsDirTestData().AppendASCII("extension"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult());

  auto resolve_function = base::MakeRefCounted<DnsResolveFunction>();
  resolve_function->set_extension(extension);
  resolve_function->set_has_callback(true);

  std::string function_arguments = base::StringPrintf(R"(["%s"])", kHostname);
  std::optional<base::Value> result(RunFunctionAndReturnSingleResult(
      resolve_function.get(), function_arguments, profile()));
  const base::Value::Dict& dict = result->GetDict();

  EXPECT_EQ(net::OK, dict.FindInt("resultCode"));

  const std::string* address = dict.FindString("address");
  ASSERT_TRUE(address);
  EXPECT_EQ(kAddress, *address);

  // Make sure the extension's NetworkAnonymizationKey was used. Do a cache only
  // DNS lookup using the expected NIK, and make sure the IP address is
  // retrieved.
  network::mojom::NetworkContext* network_context =
      profile()->GetDefaultStoragePartition()->GetNetworkContext();
  net::HostPortPair host_port_pair(kHostname, 0);
  network::mojom::ResolveHostParametersPtr params =
      network::mojom::ResolveHostParameters::New();
  // Cache only lookup.
  params->source = net::HostResolverSource::LOCAL_ONLY;
  net::SchemefulSite site = net::SchemefulSite(extension->url());
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(site);
  network::DnsLookupResult result1 =
      network::BlockingDnsLookup(network_context, host_port_pair,
                                 std::move(params), network_anonymization_key);
  EXPECT_EQ(net::OK, result1.error);
  ASSERT_EQ(1u, result1.resolved_addresses.size());
  EXPECT_EQ(kAddress, result1.resolved_addresses[0].ToStringWithoutPort());

  // Check that the entry isn't present in the cache with the empty
  // NetworkAnonymizationKey.
  params = network::mojom::ResolveHostParameters::New();
  // Cache only lookup.
  params->source = net::HostResolverSource::LOCAL_ONLY;
  network::DnsLookupResult result2 = network::BlockingDnsLookup(
      network_context, host_port_pair, std::move(params),
      net::NetworkAnonymizationKey());
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, result2.error);
}

IN_PROC_BROWSER_TEST_F(DnsApiTest, DnsExtension) {
  ASSERT_TRUE(RunExtensionTest("dns/api")) << message_;
}

}  // namespace extensions
