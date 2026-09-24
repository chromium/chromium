// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/suggestion.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/test/mojom/echo.test-mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace selection {
namespace {

class PlainSuggestion : public Suggestion {
 public:
  PlainSuggestion() = default;
  ~PlainSuggestion() override = default;

  // Suggestion:
  const std::u16string& GetLabel() const override { return label_; }
  void OnSuggestionPresented() override {}
  void OnSuggestionExecuted() override { executed_ = true; }
  mojom::ActionPtr GetAction() const override {
    return mojom::Action::NewHandoff(mojom::Handoff::New());
  }

  bool executed() const { return executed_; }

 private:
  std::u16string label_ = u"Plain";
  bool executed_ = false;
};

class EchoSuggestion : public PlainSuggestion, public ::test::mojom::Echo {
 public:
  EchoSuggestion() {
    SetInterface<::test::mojom::Echo>(
        base::BindRepeating(&EchoSuggestion::BindEcho, base::Unretained(this)));
  }
  ~EchoSuggestion() override = default;

  // ::test::mojom::Echo:
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override {
    std::move(callback).Run(input);
  }

 private:
  void BindEcho(mojo::PendingAssociatedReceiver<::test::mojom::Echo> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  mojo::AssociatedReceiver<::test::mojom::Echo> receiver_{this};
};

class SuggestionTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SuggestionTest, RegisteredBinderClaimsEndpoint) {
  EchoSuggestion suggestion;
  mojo::AssociatedRemote<::test::mojom::Echo> remote;
  suggestion.Execute(
      remote.BindNewEndpointAndPassDedicatedReceiver().PassHandle());

  EXPECT_TRUE(suggestion.executed());

  base::test::TestFuture<const std::string&> echoed;
  remote->EchoString("hello", echoed.GetCallback());
  EXPECT_EQ(echoed.Get(), "hello");
}

TEST_F(SuggestionTest, UnclaimedEndpointDisconnectsPeer) {
  PlainSuggestion suggestion;
  mojo::AssociatedRemote<::test::mojom::Echo> remote;
  suggestion.Execute(
      remote.BindNewEndpointAndPassDedicatedReceiver().PassHandle());

  EXPECT_TRUE(suggestion.executed());

  base::RunLoop loop;
  remote.set_disconnect_handler(loop.QuitClosure());
  loop.Run();
  EXPECT_FALSE(remote.is_connected());
}

TEST_F(SuggestionTest, ExecuteWithoutEndpointLeavesBinderAvailable) {
  EchoSuggestion suggestion;
  suggestion.Execute(mojo::ScopedInterfaceEndpointHandle());

  // The binder never ran, so a later call can still hand an endpoint over.
  mojo::AssociatedRemote<::test::mojom::Echo> remote;
  suggestion.Execute(
      remote.BindNewEndpointAndPassDedicatedReceiver().PassHandle());

  EXPECT_TRUE(suggestion.executed());
  base::test::TestFuture<const std::string&> echoed;
  remote->EchoString("hello", echoed.GetCallback());
  EXPECT_EQ(echoed.Get(), "hello");
}

TEST_F(SuggestionTest, InterfaceNameMatchesRegisteredInterface) {
  EXPECT_EQ(EchoSuggestion().interface_name(), ::test::mojom::Echo::Name_);
  EXPECT_TRUE(PlainSuggestion().interface_name().empty());
}

}  // namespace
}  // namespace selection
