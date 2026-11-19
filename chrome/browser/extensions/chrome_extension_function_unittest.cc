// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/response_wrapper.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

void SuccessCallback(bool* did_respond,
                     ExtensionFunction::ResponseType type,
                     mojom::ResponseWrapperPtr response_wrapper,
                     const std::string& error,
                     mojom::ExtraResponseDataPtr) {
  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded, type);
  *did_respond = true;
}

void FailCallback(bool* did_respond,
                  ExtensionFunction::ResponseType type,
                  mojom::ResponseWrapperPtr response_wrapper,
                  const std::string& error,
                  mojom::ExtraResponseDataPtr) {
  EXPECT_EQ(ExtensionFunction::ResponseType::kFailed, type);
  *did_respond = true;
}

class ValidationFunction : public ExtensionFunction {
 public:
  explicit ValidationFunction(bool should_succeed)
      : should_succeed_(should_succeed), did_respond_(false) {
    set_response_callback(base::BindOnce(
        (should_succeed ? &SuccessCallback : &FailCallback), &did_respond_));
  }

  ResponseAction Run() override {
    EXPECT_TRUE(should_succeed_);
    return RespondNow(NoArguments());
  }

  bool did_respond() { return did_respond_; }

 private:
  ~ValidationFunction() override = default;
  bool should_succeed_;
  bool did_respond_;
};
}  // namespace

using ChromeExtensionFunctionUnitTest = ExtensionServiceTestBase;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SimpleFunctionTest DISABLED_SimpleFunctionTest
#else
#define MAYBE_SimpleFunctionTest SimpleFunctionTest
#endif
TEST_F(ChromeExtensionFunctionUnitTest, MAYBE_SimpleFunctionTest) {
  scoped_refptr<ValidationFunction> function(new ValidationFunction(true));
  function->RunWithValidation().Execute();
  EXPECT_TRUE(function->did_respond());
}

TEST_F(ChromeExtensionFunctionUnitTest, BrowserShutdownValidationFunctionTest) {
  TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
  scoped_refptr<ValidationFunction> function(new ValidationFunction(false));
  function->RunWithValidation().Execute();
  TestingBrowserProcess::GetGlobal()->SetShuttingDown(false);
  EXPECT_TRUE(function->did_respond());
}

// Verifies that destroying the ExtensionFunction without responding is ok if
// the extension has been unloaded.
TEST_F(ChromeExtensionFunctionUnitTest, DestructionWithoutResponseOnUnload) {
  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> extension = ExtensionBuilder("foo").Build();
  registrar()->AddExtension(extension);
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  auto function = base::MakeRefCounted<ValidationFunction>(false);
  function->set_extension(extension);
  function->SetBrowserContextForTesting(browser_context());

  registrar()->DisableExtension(extension->id(),
                                {disable_reason::DISABLE_USER_ACTION});
  ASSERT_TRUE(registry()->disabled_extensions().Contains(extension->id()));

  // Destroying the extension function without responding if the extension has
  // been unloaded should not cause a crash.
  function.reset();
}

namespace {

// Minimal ExtensionFunction used to reproduce the BrowserContext-shutdown
// use-after-free. It only needs to subscribe to the per-context shutdown
// notifier (via SetDispatcher()) and to count its own destructions.
class ShutdownRaceTestFunction : public ExtensionFunction {
 public:
  ShutdownRaceTestFunction() = default;

  ResponseAction Run() override { return RespondNow(NoArguments()); }

 protected:
  ~ShutdownRaceTestFunction() override = default;
};

}  // namespace

// Regression test for a double-free of an ExtensionFunction during
// BrowserContext shutdown. See the explanation of the failure mode in
// ExtensionFunction::Shutdown.
TEST_F(ChromeExtensionFunctionUnitTest,
       ShutdownDoesNotResurrectDeletedFunction) {
  InitializeEmptyExtensionService();

  // A dedicated off-the-record profile we can destroy deterministically to fire
  // the ExtensionFunction shutdown notifier (kOwnInstance => its own notifier).
  Profile::OTRProfileID otr_id =
      Profile::OTRProfileID::CreateUniqueForTesting();
  Profile* otr_profile =
      profile()->GetOffTheRecordProfile(otr_id, /*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);

  auto dispatcher = std::make_unique<ExtensionFunctionDispatcher>(otr_profile);

  auto function = base::MakeRefCounted<ShutdownRaceTestFunction>();
  function->SetName("shutdownRaceTest");
  function->set_response_callback(base::DoNothing());
  // Subscribes to the per-context shutdown notifier (with base::Unretained).
  function->SetDispatcher(dispatcher->AsWeakPtr());

  // Release the last reference off the UI thread so the DeleteOnUIThread
  // deleter defers destruction via DeleteSoon. Afterwards the object is a
  // zombie: refcount 0, a DeleteSoon task queued on the UI thread, destructor
  // not yet run, and the shutdown subscription still registered.
  base::WaitableEvent released(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](scoped_refptr<ExtensionFunction> f, base::WaitableEvent* done) {
            f.reset();
            done->Signal();
          },
          std::move(function), &released));
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow;
    released.Wait();
  }

  // Tear down the context, firing the shutdown notifier, which invokes
  // ExtensionFunction::Shutdown() on the zombie.
  profile()->DestroyOffTheRecordProfile(otr_profile);

  // Run the queued DeleteSoon, which must be the single deletion.
  content::RunAllTasksUntilIdle();

  dispatcher.reset();
}

#if DCHECK_IS_ON()
using ChromeExtensionFunctionDeathTest = ChromeExtensionFunctionUnitTest;

// Verify that destroying the extension function without responding causes a
// DCHECK failure.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DestructionWithoutResponse DISABLED_DestructionWithoutResponse
#else
#define MAYBE_DestructionWithoutResponse DestructionWithoutResponse
#endif
TEST_F(ChromeExtensionFunctionDeathTest, MAYBE_DestructionWithoutResponse) {
  ASSERT_DEATH(
      {
        InitializeEmptyExtensionService();
        scoped_refptr<const Extension> extension =
            ExtensionBuilder("foo").Build();
        registrar()->AddExtension(extension);

        ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

        auto function = base::MakeRefCounted<ValidationFunction>(false);
        function->set_extension(extension);
        function.reset();
      },
      "");
}
#endif  // DCHECK_IS_ON()

}  // namespace extensions
