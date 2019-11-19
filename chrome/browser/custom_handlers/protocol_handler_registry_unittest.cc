// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

std::unique_ptr<base::DictionaryValue> GetProtocolHandlerValue(
    const std::string& protocol,
    const std::string& url) {
  auto value = std::make_unique<base::DictionaryValue>();
  value->SetString("protocol", protocol);
  value->SetString("url", url);
  return value;
}

std::unique_ptr<base::DictionaryValue> GetProtocolHandlerValueWithDefault(
    const std::string& protocol,
    const std::string& url,
    bool is_default) {
  std::unique_ptr<base::DictionaryValue> value =
      GetProtocolHandlerValue(protocol, url);
  value->SetBoolean("default", is_default);
  return value;
}

class FakeDelegate : public ProtocolHandlerRegistry::Delegate {
 public:
  FakeDelegate() : force_os_failure_(false) {}
  ~FakeDelegate() override {}
  void RegisterExternalHandler(const std::string& protocol) override {
    ASSERT_TRUE(
        registered_protocols_.find(protocol) == registered_protocols_.end());
    registered_protocols_.insert(protocol);
  }

  void DeregisterExternalHandler(const std::string& protocol) override {
    registered_protocols_.erase(protocol);
  }

  void RegisterWithOSAsDefaultClient(
      const std::string& protocol,
      shell_integration::DefaultWebClientWorkerCallback callback) override {
    // Do as-if the registration has to run on another sequence and post back
    // the result with a task to the current thread.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(callback, force_os_failure_
                                     ? shell_integration::NOT_DEFAULT
                                     : shell_integration::IS_DEFAULT));

    if (!force_os_failure_)
      os_registered_protocols_.insert(protocol);
  }

  bool IsExternalHandlerRegistered(const std::string& protocol) override {
    return registered_protocols_.find(protocol) != registered_protocols_.end();
  }

  bool IsFakeRegisteredWithOS(const std::string& protocol) {
    return os_registered_protocols_.find(protocol) !=
        os_registered_protocols_.end();
  }

  void Reset() {
    registered_protocols_.clear();
    os_registered_protocols_.clear();
    force_os_failure_ = false;
  }

  void set_force_os_failure(bool force) { force_os_failure_ = force; }

  bool force_os_failure() { return force_os_failure_; }

 private:
  std::set<std::string> registered_protocols_;
  std::set<std::string> os_registered_protocols_;
  bool force_os_failure_;
};

class ProtocolHandlerChangeListener : public ProtocolHandlerRegistry::Observer {
 public:
  explicit ProtocolHandlerChangeListener(ProtocolHandlerRegistry* registry) {
    registry_observer_.Add(registry);
  }
  ~ProtocolHandlerChangeListener() override = default;

  int events() { return events_; }
  bool notified() { return events_ > 0; }
  void Clear() { events_ = 0; }

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override { ++events_; }

 private:
  int events_ = 0;

  ScopedObserver<ProtocolHandlerRegistry, ProtocolHandlerRegistry::Observer>
      registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerChangeListener);
};

class QueryProtocolHandlerOnChange : public ProtocolHandlerRegistry::Observer {
 public:
  explicit QueryProtocolHandlerOnChange(ProtocolHandlerRegistry* registry)
      : local_registry_(registry) {
    registry_observer_.Add(registry);
  }

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override {
    std::vector<std::string> output;
    local_registry_->GetRegisteredProtocols(&output);
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  ProtocolHandlerRegistry* local_registry_;
  bool called_ = false;

  ScopedObserver<ProtocolHandlerRegistry, ProtocolHandlerRegistry::Observer>
      registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(QueryProtocolHandlerOnChange);
};

}  // namespace

class ProtocolHandlerRegistryTest : public testing::Test {
 protected:
  ProtocolHandlerRegistryTest()
      : test_protocol_handler_(CreateProtocolHandler("news", "news")) {}

  FakeDelegate* delegate() const { return delegate_; }
  ProtocolHandlerRegistry* registry() { return registry_.get(); }
  TestingProfile* profile() const { return profile_.get(); }
  const ProtocolHandler& test_protocol_handler() const {
    return test_protocol_handler_;
  }

  ProtocolHandler CreateProtocolHandler(const std::string& protocol,
                                        const GURL& url) {
    return ProtocolHandler::CreateProtocolHandler(protocol, url);
  }

  ProtocolHandler CreateProtocolHandler(const std::string& protocol,
                                        const std::string& name) {
    return CreateProtocolHandler(protocol, GURL("http://" + name + "/%s"));
  }

  void RecreateRegistry(bool initialize) {
    TeadDownRegistry();
    SetUpRegistry(initialize);
  }

  int InPrefHandlerCount() {
    const base::ListValue* in_pref_handlers =
        profile()->GetPrefs()->GetList(prefs::kRegisteredProtocolHandlers);
    return static_cast<int>(in_pref_handlers->GetSize());
  }

  int InMemoryHandlerCount() {
    int in_memory_handler_count = 0;
    auto it = registry()->protocol_handlers_.begin();
    for (; it != registry()->protocol_handlers_.end(); ++it)
      in_memory_handler_count += it->second.size();
    return in_memory_handler_count;
  }

  int InPrefIgnoredHandlerCount() {
    const base::ListValue* in_pref_ignored_handlers =
        profile()->GetPrefs()->GetList(prefs::kIgnoredProtocolHandlers);
    return static_cast<int>(in_pref_ignored_handlers->GetSize());
  }

  int InMemoryIgnoredHandlerCount() {
    int in_memory_ignored_handler_count = 0;
    auto it = registry()->ignored_protocol_handlers_.begin();
    for (; it != registry()->ignored_protocol_handlers_.end(); ++it)
      in_memory_ignored_handler_count++;
    return in_memory_ignored_handler_count;
  }

  // Returns a new registry, initializing it if |initialize| is true.
  // Caller assumes ownership for the object
  void SetUpRegistry(bool initialize) {
    auto delegate = std::make_unique<FakeDelegate>();
    delegate_ = delegate.get();
    registry_.reset(
        new ProtocolHandlerRegistry(profile(), std::move(delegate)));
    if (initialize) registry_->InitProtocolSettings();
  }

  void TeadDownRegistry() {
    registry_->Shutdown();
    registry_.reset();
    // Registry owns the delegate_ it handles deletion of that object.
  }

  void SetUp() override {
    profile_.reset(new TestingProfile());
    CHECK(profile_->GetPrefs());
    SetUpRegistry(true);
    test_protocol_handler_ =
        CreateProtocolHandler("news", GURL("http://test.com/%s"));
  }

  void TearDown() override { TeadDownRegistry(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  FakeDelegate* delegate_;  // Registry assumes ownership of delegate_.
  std::unique_ptr<ProtocolHandlerRegistry> registry_;
  ProtocolHandler test_protocol_handler_;
};

TEST_F(ProtocolHandlerRegistryTest, AcceptProtocolHandlerHandlesProtocol) {
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, DeniedProtocolIsntHandledUntilAccepted) {
  registry()->OnDenyRegisterProtocolHandler(test_protocol_handler());
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, ClearDefaultMakesProtocolNotHandled) {
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  registry()->ClearDefault("news");
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  ASSERT_TRUE(registry()->GetHandlerFor("news").IsEmpty());
}

TEST_F(ProtocolHandlerRegistryTest, DisableDeregistersProtocolHandlers) {
  ASSERT_FALSE(delegate()->IsExternalHandlerRegistered("news"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(delegate()->IsExternalHandlerRegistered("news"));

  registry()->Disable();
  ASSERT_FALSE(delegate()->IsExternalHandlerRegistered("news"));
  registry()->Enable();
  ASSERT_TRUE(delegate()->IsExternalHandlerRegistered("news"));
}

TEST_F(ProtocolHandlerRegistryTest, IgnoreProtocolHandler) {
  registry()->OnIgnoreRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsIgnored(test_protocol_handler()));

  registry()->RemoveIgnoredHandler(test_protocol_handler());
  ASSERT_FALSE(registry()->IsIgnored(test_protocol_handler()));
}

TEST_F(ProtocolHandlerRegistryTest, IgnoreEquivalentProtocolHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", GURL("http://test/%s"));
  ProtocolHandler ph2 = CreateProtocolHandler("news", GURL("http://test/%s"));

  registry()->OnIgnoreRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsIgnored(ph1));
  ASSERT_TRUE(registry()->HasIgnoredEquivalent(ph2));

  registry()->RemoveIgnoredHandler(ph1);
  ASSERT_FALSE(registry()->IsIgnored(ph1));
  ASSERT_FALSE(registry()->HasIgnoredEquivalent(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, SaveAndLoad) {
  ProtocolHandler stuff_protocol_handler(
      CreateProtocolHandler("stuff", "stuff"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  registry()->OnIgnoreRegisterProtocolHandler(stuff_protocol_handler);

  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
  ASSERT_TRUE(registry()->IsIgnored(stuff_protocol_handler));
  delegate()->Reset();
  RecreateRegistry(true);
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
  ASSERT_TRUE(registry()->IsIgnored(stuff_protocol_handler));
}

TEST_F(ProtocolHandlerRegistryTest, Encode) {
  base::Time now = base::Time::Now();
  ProtocolHandler handler("news", GURL("http://example.com"), now);
  auto value = handler.Encode();
  ProtocolHandler recreated =
      ProtocolHandler::CreateProtocolHandler(value.get());
  EXPECT_EQ("news", recreated.protocol());
  EXPECT_EQ(GURL("http://example.com"), recreated.url());
  EXPECT_EQ(now, recreated.last_modified());
}

TEST_F(ProtocolHandlerRegistryTest, GetHandlersBetween) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago = now - base::TimeDelta::FromHours(1);
  base::Time two_hours_ago = now - base::TimeDelta::FromHours(2);
  ProtocolHandler handler1("bitcoin", GURL("http://example.com"),
                           two_hours_ago);
  ProtocolHandler handler2("geo", GURL("http://example.com"), one_hour_ago);
  ProtocolHandler handler3("im", GURL("http://example.com"), now);
  registry()->OnAcceptRegisterProtocolHandler(handler1);
  registry()->OnAcceptRegisterProtocolHandler(handler2);
  registry()->OnAcceptRegisterProtocolHandler(handler3);

  EXPECT_EQ(
      std::vector<ProtocolHandler>({handler1, handler2, handler3}),
      registry()->GetUserDefinedHandlers(base::Time(), base::Time::Max()));
  EXPECT_EQ(
      std::vector<ProtocolHandler>({handler2, handler3}),
      registry()->GetUserDefinedHandlers(one_hour_ago, base::Time::Max()));
  EXPECT_EQ(std::vector<ProtocolHandler>({handler1, handler2}),
            registry()->GetUserDefinedHandlers(base::Time(), now));
}

TEST_F(ProtocolHandlerRegistryTest, ClearHandlersBetween) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago = now - base::TimeDelta::FromHours(1);
  base::Time two_hours_ago = now - base::TimeDelta::FromHours(2);
  GURL url("http://example.com");
  ProtocolHandler handler1("bitcoin", url, two_hours_ago);
  ProtocolHandler handler2("geo", url, one_hour_ago);
  ProtocolHandler handler3("im", url, now);
  ProtocolHandler ignored1("irc", url, two_hours_ago);
  ProtocolHandler ignored2("ircs", url, one_hour_ago);
  ProtocolHandler ignored3("magnet", url, now);
  registry()->OnAcceptRegisterProtocolHandler(handler1);
  registry()->OnAcceptRegisterProtocolHandler(handler2);
  registry()->OnAcceptRegisterProtocolHandler(handler3);
  registry()->OnIgnoreRegisterProtocolHandler(ignored1);
  registry()->OnIgnoreRegisterProtocolHandler(ignored2);
  registry()->OnIgnoreRegisterProtocolHandler(ignored3);

  EXPECT_TRUE(registry()->IsHandledProtocol("bitcoin"));
  EXPECT_TRUE(registry()->IsHandledProtocol("geo"));
  EXPECT_TRUE(registry()->IsHandledProtocol("im"));
  EXPECT_TRUE(registry()->IsIgnored(ignored1));
  EXPECT_TRUE(registry()->IsIgnored(ignored2));
  EXPECT_TRUE(registry()->IsIgnored(ignored3));

  // Delete handler2 and ignored2.
  registry()->ClearUserDefinedHandlers(one_hour_ago, now);
  EXPECT_TRUE(registry()->IsHandledProtocol("bitcoin"));
  EXPECT_FALSE(registry()->IsHandledProtocol("geo"));
  EXPECT_TRUE(registry()->IsHandledProtocol("im"));
  EXPECT_TRUE(registry()->IsIgnored(ignored1));
  EXPECT_FALSE(registry()->IsIgnored(ignored2));
  EXPECT_TRUE(registry()->IsIgnored(ignored3));

  // Delete all.
  registry()->ClearUserDefinedHandlers(base::Time(), base::Time::Max());
  EXPECT_FALSE(registry()->IsHandledProtocol("bitcoin"));
  EXPECT_FALSE(registry()->IsHandledProtocol("geo"));
  EXPECT_FALSE(registry()->IsHandledProtocol("im"));
  EXPECT_FALSE(registry()->IsIgnored(ignored1));
  EXPECT_FALSE(registry()->IsIgnored(ignored2));
  EXPECT_FALSE(registry()->IsIgnored(ignored3));
}

TEST_F(ProtocolHandlerRegistryTest, TestEnabledDisabled) {
  registry()->Disable();
  ASSERT_FALSE(registry()->enabled());
  registry()->Enable();
  ASSERT_TRUE(registry()->enabled());
}

TEST_F(ProtocolHandlerRegistryTest,
    DisallowRegisteringExternallyHandledProtocols) {
  delegate()->RegisterExternalHandler("news");
  ASSERT_FALSE(registry()->CanSchemeBeOverridden("news"));
}

TEST_F(ProtocolHandlerRegistryTest, RemovingHandlerMeansItCanBeAddedAgain) {
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->CanSchemeBeOverridden("news"));
  registry()->RemoveHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->CanSchemeBeOverridden("news"));
}

TEST_F(ProtocolHandlerRegistryTest, TestStartsAsDefault) {
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsDefault(test_protocol_handler()));
}

TEST_F(ProtocolHandlerRegistryTest, TestClearDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->ClearDefault("news");
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_FALSE(registry()->IsDefault(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestGetHandlerFor) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_EQ(ph2, registry()->GetHandlerFor("news"));
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, TestMostRecentHandlerIsDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestOnAcceptRegisterProtocolHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsDefault(ph1));
  ASSERT_FALSE(registry()->IsDefault(ph2));

  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestDefaultSaveLoad) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnDenyRegisterProtocolHandler(ph1);
  registry()->OnDenyRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->Disable();

  RecreateRegistry(true);

  ASSERT_FALSE(registry()->enabled());
  registry()->Enable();
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));

  RecreateRegistry(true);
  ASSERT_TRUE(registry()->enabled());
}

TEST_F(ProtocolHandlerRegistryTest, TestRemoveHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph1);

  registry()->RemoveHandler(ph1);
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));

  registry()->OnIgnoreRegisterProtocolHandler(ph1);
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_TRUE(registry()->IsIgnored(ph1));

  registry()->RemoveHandler(ph1);
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_FALSE(registry()->IsIgnored(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestIsRegistered) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  ASSERT_TRUE(registry()->IsRegistered(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestIsEquivalentRegistered) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", GURL("http://test/%s"));
  ProtocolHandler ph2 = CreateProtocolHandler("news", GURL("http://test/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);

  ASSERT_TRUE(registry()->IsRegistered(ph1));
  ASSERT_TRUE(registry()->HasRegisteredEquivalent(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestSilentlyRegisterHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", GURL("http://test/1/%s"));
  ProtocolHandler ph2 = CreateProtocolHandler("news", GURL("http://test/2/%s"));
  ProtocolHandler ph3 = CreateProtocolHandler("ignore", GURL("http://test/%s"));
  ProtocolHandler ph4 = CreateProtocolHandler("ignore", GURL("http://test/%s"));

  ASSERT_FALSE(registry()->SilentlyHandleRegisterHandlerRequest(ph1));
  ASSERT_FALSE(registry()->IsRegistered(ph1));

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsRegistered(ph1));

  ASSERT_TRUE(registry()->SilentlyHandleRegisterHandlerRequest(ph2));
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_TRUE(registry()->IsRegistered(ph2));

  ASSERT_FALSE(registry()->SilentlyHandleRegisterHandlerRequest(ph3));
  ASSERT_FALSE(registry()->IsRegistered(ph3));

  registry()->OnIgnoreRegisterProtocolHandler(ph3);
  ASSERT_FALSE(registry()->IsRegistered(ph3));
  ASSERT_TRUE(registry()->IsIgnored(ph3));

  ASSERT_TRUE(registry()->SilentlyHandleRegisterHandlerRequest(ph4));
  ASSERT_FALSE(registry()->IsRegistered(ph4));
  ASSERT_TRUE(registry()->HasIgnoredEquivalent(ph4));
}

TEST_F(ProtocolHandlerRegistryTest, TestRemoveHandlerRemovesDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("news", "test3");

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->RemoveHandler(ph1);
  ASSERT_FALSE(registry()->IsDefault(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestGetHandlersFor) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("news", "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);

  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      registry()->GetHandlersFor("news");
  ASSERT_EQ(static_cast<size_t>(3), handlers.size());

  ASSERT_EQ(ph3, handlers[0]);
  ASSERT_EQ(ph2, handlers[1]);
  ASSERT_EQ(ph1, handlers[2]);
}

TEST_F(ProtocolHandlerRegistryTest, TestGetRegisteredProtocols) {
  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(0), protocols.size());

  registry()->GetHandlersFor("news");

  protocols.clear();
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(0), protocols.size());
}

TEST_F(ProtocolHandlerRegistryTest, TestIsHandledProtocol) {
  registry()->GetHandlersFor("news");
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, TestObserver) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandlerChangeListener counter(registry());

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(counter.notified());
  counter.Clear();

  registry()->Disable();
  ASSERT_TRUE(counter.notified());
  counter.Clear();

  registry()->Enable();
  ASSERT_TRUE(counter.notified());
  counter.Clear();

  registry()->RemoveHandler(ph1);
  ASSERT_TRUE(counter.notified());
  counter.Clear();
}

TEST_F(ProtocolHandlerRegistryTest, TestReentrantObserver) {
  QueryProtocolHandlerOnChange queryer(registry());
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(queryer.called());
}

TEST_F(ProtocolHandlerRegistryTest, TestProtocolsWithNoDefaultAreHandled) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->ClearDefault("news");
  std::vector<std::string> handled_protocols;
  registry()->GetRegisteredProtocols(&handled_protocols);
  ASSERT_EQ(static_cast<size_t>(1), handled_protocols.size());
  ASSERT_EQ("news", handled_protocols[0]);
}

TEST_F(ProtocolHandlerRegistryTest, TestDisablePreventsHandling) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
  registry()->Disable();
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, TestOSRegistration) {
  ProtocolHandler ph_do1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph_do2 = CreateProtocolHandler("news", "test2");
  ProtocolHandler ph_dont = CreateProtocolHandler("im", "test3");

  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("news"));
  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("im"));

  registry()->OnAcceptRegisterProtocolHandler(ph_do1);
  registry()->OnDenyRegisterProtocolHandler(ph_dont);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(delegate()->IsFakeRegisteredWithOS("news"));
  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("im"));

  // This should not register with the OS, if it does the delegate
  // will assert for us. We don't need to wait for the message loop
  // as it should not go through to the shell worker.
  registry()->OnAcceptRegisterProtocolHandler(ph_do2);
}

#if defined(OS_LINUX)
// TODO(benwells): When Linux support is more reliable and
// http://crbug.com/88255 is fixed this test will pass.
#define MAYBE_TestOSRegistrationFailure DISABLED_TestOSRegistrationFailure
#else
#define MAYBE_TestOSRegistrationFailure TestOSRegistrationFailure
#endif

TEST_F(ProtocolHandlerRegistryTest, MAYBE_TestOSRegistrationFailure) {
  ProtocolHandler ph_do = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph_dont = CreateProtocolHandler("im", "test2");

  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  ASSERT_FALSE(registry()->IsHandledProtocol("im"));

  registry()->OnAcceptRegisterProtocolHandler(ph_do);
  base::RunLoop().RunUntilIdle();

  delegate()->set_force_os_failure(true);
  registry()->OnAcceptRegisterProtocolHandler(ph_dont);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
  ASSERT_EQ(static_cast<size_t>(1), registry()->GetHandlersFor("news").size());
  ASSERT_FALSE(registry()->IsHandledProtocol("im"));
  ASSERT_EQ(static_cast<size_t>(1), registry()->GetHandlersFor("im").size());
}

TEST_F(ProtocolHandlerRegistryTest, TestRemovingDefaultFallsBackToOldDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("mailto", "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);

  ASSERT_TRUE(registry()->IsDefault(ph3));
  registry()->RemoveHandler(ph3);
  ASSERT_TRUE(registry()->IsDefault(ph2));
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  ASSERT_TRUE(registry()->IsDefault(ph3));
  registry()->RemoveHandler(ph2);
  ASSERT_TRUE(registry()->IsDefault(ph3));
  registry()->RemoveHandler(ph3);
  ASSERT_TRUE(registry()->IsDefault(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestRemovingDefaultDoesntChangeHandlers) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("mailto", "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  registry()->RemoveHandler(ph3);

  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      registry()->GetHandlersFor("mailto");
  ASSERT_EQ(static_cast<size_t>(2), handlers.size());

  ASSERT_EQ(ph2, handlers[0]);
  ASSERT_EQ(ph1, handlers[1]);
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceHandler) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("http://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("http://test.com/updated-url/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->AttemptReplace(ph2));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph2.url());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceNonDefaultHandler) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("http://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("http://test.com/updated-url/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("mailto", GURL("http://else.com/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  ASSERT_TRUE(registry()->AttemptReplace(ph2));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph3.url());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceRemovesStaleHandlers) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("http://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("http://test.com/updated-url/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("mailto", GURL("http://test.com/third/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  // This should replace the previous two handlers.
  ASSERT_TRUE(registry()->AttemptReplace(ph3));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph3.url());
  registry()->RemoveHandler(ph3);
  ASSERT_TRUE(registry()->GetHandlerFor("mailto").IsEmpty());
}

TEST_F(ProtocolHandlerRegistryTest, TestIsSameOrigin) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("http://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("http://test.com/updated-url/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("mailto", GURL("http://other.com/%s"));
  ASSERT_EQ(ph1.url().GetOrigin() == ph2.url().GetOrigin(),
      ph1.IsSameOrigin(ph2));
  ASSERT_EQ(ph1.url().GetOrigin() == ph2.url().GetOrigin(),
      ph2.IsSameOrigin(ph1));
  ASSERT_EQ(ph2.url().GetOrigin() == ph3.url().GetOrigin(),
      ph2.IsSameOrigin(ph3));
  ASSERT_EQ(ph3.url().GetOrigin() == ph2.url().GetOrigin(),
      ph3.IsSameOrigin(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestInstallDefaultHandler) {
  RecreateRegistry(false);
  registry()->AddPredefinedHandler(
      CreateProtocolHandler("news", GURL("http://test.com/%s")));
  registry()->InitProtocolSettings();
  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(1), protocols.size());
  EXPECT_TRUE(registry()->IsHandledProtocol("news"));
  auto handlers =
      registry()->GetUserDefinedHandlers(base::Time(), base::Time::Max());
  EXPECT_TRUE(handlers.empty());
  registry()->ClearUserDefinedHandlers(base::Time(), base::Time::Max());
  EXPECT_TRUE(registry()->IsHandledProtocol("news"));
}

#define URL_p1u1 "http://p1u1.com/%s"
#define URL_p1u2 "http://p1u2.com/%s"
#define URL_p1u3 "http://p1u3.com/%s"
#define URL_p2u1 "http://p2u1.com/%s"
#define URL_p2u2 "http://p2u2.com/%s"
#define URL_p3u1 "http://p3u1.com/%s"

TEST_F(ProtocolHandlerRegistryTest, TestPrefPolicyOverlapRegister) {
  base::ListValue handlers_registered_by_pref;
  base::ListValue handlers_registered_by_policy;

  handlers_registered_by_pref.Append(
      GetProtocolHandlerValueWithDefault("news", URL_p1u2, true));
  handlers_registered_by_pref.Append(
      GetProtocolHandlerValueWithDefault("news", URL_p1u1, true));
  handlers_registered_by_pref.Append(
      GetProtocolHandlerValueWithDefault("news", URL_p1u2, false));

  handlers_registered_by_policy.Append(
      GetProtocolHandlerValueWithDefault("news", URL_p1u1, false));
  handlers_registered_by_policy.Append(
      GetProtocolHandlerValueWithDefault("mailto", URL_p3u1, true));

  profile()->GetPrefs()->Set(prefs::kRegisteredProtocolHandlers,
                             handlers_registered_by_pref);
  profile()->GetPrefs()->Set(prefs::kPolicyRegisteredProtocolHandlers,
                             handlers_registered_by_policy);
  registry()->InitProtocolSettings();

  // Duplicate p1u2 eliminated in memory but not yet saved in pref
  ProtocolHandler p1u1 = CreateProtocolHandler("news", GURL(URL_p1u1));
  ProtocolHandler p1u2 = CreateProtocolHandler("news", GURL(URL_p1u2));
  ASSERT_EQ(InPrefHandlerCount(), 3);
  ASSERT_EQ(InMemoryHandlerCount(), 3);
  ASSERT_TRUE(registry()->IsDefault(p1u1));
  ASSERT_FALSE(registry()->IsDefault(p1u2));

  ProtocolHandler p2u1 = CreateProtocolHandler("im", GURL(URL_p2u1));
  registry()->OnDenyRegisterProtocolHandler(p2u1);

  // Duplicate p1u2 saved in pref and a new handler added to pref and memory
  ASSERT_EQ(InPrefHandlerCount(), 3);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_FALSE(registry()->IsDefault(p2u1));

  registry()->RemoveHandler(p1u1);

  // p1u1 removed from user pref but not from memory due to policy.
  ASSERT_EQ(InPrefHandlerCount(), 2);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_TRUE(registry()->IsDefault(p1u1));

  ProtocolHandler p3u1 = CreateProtocolHandler("mailto", GURL(URL_p3u1));
  registry()->RemoveHandler(p3u1);

  // p3u1 not removed from memory due to policy and it was never in pref.
  ASSERT_EQ(InPrefHandlerCount(), 2);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_TRUE(registry()->IsDefault(p3u1));

  registry()->RemoveHandler(p1u2);

  // p1u2 removed from user pref and memory.
  ASSERT_EQ(InPrefHandlerCount(), 1);
  ASSERT_EQ(InMemoryHandlerCount(), 3);
  ASSERT_TRUE(registry()->IsDefault(p1u1));

  ProtocolHandler p1u3 = CreateProtocolHandler("news", GURL(URL_p1u3));
  registry()->OnAcceptRegisterProtocolHandler(p1u3);

  // p1u3 added to pref and memory.
  ASSERT_EQ(InPrefHandlerCount(), 2);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_FALSE(registry()->IsDefault(p1u1));
  ASSERT_TRUE(registry()->IsDefault(p1u3));

  registry()->RemoveHandler(p1u3);

  // p1u3 the default handler for p1 removed from user pref and memory.
  ASSERT_EQ(InPrefHandlerCount(), 1);
  ASSERT_EQ(InMemoryHandlerCount(), 3);
  ASSERT_FALSE(registry()->IsDefault(p1u3));
  ASSERT_TRUE(registry()->IsDefault(p1u1));
  ASSERT_TRUE(registry()->IsDefault(p3u1));
  ASSERT_FALSE(registry()->IsDefault(p2u1));
}

TEST_F(ProtocolHandlerRegistryTest, TestPrefPolicyOverlapIgnore) {
  base::ListValue handlers_ignored_by_pref;
  base::ListValue handlers_ignored_by_policy;

  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("news", URL_p1u1));
  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("news", URL_p1u2));
  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("news", URL_p1u2));
  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("mailto", URL_p3u1));

  handlers_ignored_by_policy.Append(GetProtocolHandlerValue("news", URL_p1u2));
  handlers_ignored_by_policy.Append(GetProtocolHandlerValue("news", URL_p1u3));
  handlers_ignored_by_policy.Append(GetProtocolHandlerValue("im", URL_p2u1));

  profile()->GetPrefs()->Set(prefs::kIgnoredProtocolHandlers,
                             handlers_ignored_by_pref);
  profile()->GetPrefs()->Set(prefs::kPolicyIgnoredProtocolHandlers,
                             handlers_ignored_by_policy);
  registry()->InitProtocolSettings();

  // Duplicate p1u2 eliminated in memory but not yet saved in pref
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 4);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 5);

  ProtocolHandler p2u2 = CreateProtocolHandler("im", GURL(URL_p2u2));
  registry()->OnIgnoreRegisterProtocolHandler(p2u2);

  // Duplicate p1u2 eliminated in pref, p2u2 added to pref and memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 4);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 6);

  ProtocolHandler p2u1 = CreateProtocolHandler("im", GURL(URL_p2u1));
  registry()->RemoveIgnoredHandler(p2u1);

  // p2u1 installed by policy so cant be removed.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 4);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 6);

  ProtocolHandler p1u2 = CreateProtocolHandler("news", GURL(URL_p1u2));
  registry()->RemoveIgnoredHandler(p1u2);

  // p1u2 installed by policy and pref so it is removed from pref and not from
  // memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 3);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 6);

  ProtocolHandler p1u1 = CreateProtocolHandler("news", GURL(URL_p1u1));
  registry()->RemoveIgnoredHandler(p1u1);

  // p1u1 installed by pref so it is removed from pref and memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 2);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 5);

  registry()->RemoveIgnoredHandler(p2u2);

  // p2u2 installed by user so it is removed from pref and memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 1);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 4);

  registry()->OnIgnoreRegisterProtocolHandler(p2u1);

  // p2u1 installed by user but it is already installed by policy, so it is
  // added to pref.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 2);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 4);

  registry()->RemoveIgnoredHandler(p2u1);

  // p2u1 installed by user and policy, so it is removed from pref alone.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 1);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 4);
}

TEST_F(ProtocolHandlerRegistryTest, TestURIPercentEncoding) {
  ProtocolHandler ph =
      CreateProtocolHandler("web+custom", GURL("https://test.com/url=%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph);

  // Normal case.
  GURL translated_url = ph.TranslateUrl(GURL("web+custom://custom/handler"));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%2Fhandler"));

  // Percent-encoding.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/%20handler"));
  ASSERT_EQ(
      translated_url,
      GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%2F%2520handler"));

  // Space character.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom handler"));
  // TODO(mgiuca): Check whether this(' ') should be encoded as '%20'.
  ASSERT_EQ(translated_url,
            GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom+handler"));

  // Query parameters.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom?foo=bar&bar=baz"));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/"
                 "url=web%2Bcustom%3A%2F%2Fcustom%3Ffoo%3Dbar%26bar%3Dbaz"));

  // Non-ASCII characters.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/<>`{}#?\"'😂"));
  ASSERT_EQ(translated_url, GURL("https://test.com/"
                                 "url=web%2Bcustom%3A%2F%2Fcustom%2F%3C%3E%60%"
                                 "7B%7D%23%3F%22'%25F0%259F%2598%2582"));

  // C0 characters. GURL constructor encodes U+001F as "%1F" first, because
  // U+001F is an illegal char. Then the protocol handler translator encodes it
  // to "%251F" again. That's why the expected result has double-encoded URL.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/\x1fhandler"));
  ASSERT_EQ(
      translated_url,
      GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%2F%251Fhandler"));

  // Control characters.
  // TODO(crbug.com/809852): Check why non-special URLs don't encode any
  // characters above U+001F.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/\x7Fhandler"));
  ASSERT_EQ(
      translated_url,
      GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%2F%7Fhandler"));

  // Path percent-encode set.
  translated_url =
      ph.TranslateUrl(GURL("web+custom://custom/handler=#download"));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/"
                 "url=web%2Bcustom%3A%2F%2Fcustom%2Fhandler%3D%23download"));

  // Userinfo percent-encode set.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/handler:@id="));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/"
                 "url=web%2Bcustom%3A%2F%2Fcustom%2Fhandler%3A%40id%3D"));
}

TEST_F(ProtocolHandlerRegistryTest, TestMultiplePlaceholders) {
  ProtocolHandler ph =
      CreateProtocolHandler("news", GURL("http://example.com/%s/url=%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph);

  GURL translated_url = ph.TranslateUrl(GURL("test:duplicated_placeholders"));

  // When URL contains multiple placeholders, only the first placeholder should
  // be changed to the given URL.
  ASSERT_EQ(translated_url,
            GURL("http://example.com/test%3Aduplicated_placeholders/url=%s"));
}

TEST_F(ProtocolHandlerRegistryTest, InvalidHandlers) {
  // Invalid protocol.
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("foo", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("foo"));
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("web", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("web"));
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("web+", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("web+"));
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("https", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("https"));

  // Invalid handler URL.
  // data: URL.
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "news",
      GURL("data:text/html,<html><body><b>hello world</b></body></html>%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  // ftp:// URL.
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("news", GURL("ftp://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  // blob:// URL
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "news", GURL("blob:https://www.google.com/"
                   "f2d8c47d-17d0-4bf5-8f0a-76e42cbed3bf/%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, ExtensionHandler) {
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "news",
      GURL("chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/test.html")));
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
}
