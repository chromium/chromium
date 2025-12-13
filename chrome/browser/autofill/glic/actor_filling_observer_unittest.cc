// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/actor_filling_observer.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using test::MakeFieldGlobalId;
using test::MakeFormGlobalId;

class ActorFillingObserverTest : public ::testing::Test,
                                 public WithTestAutofillClientDriverManager<> {
 public:
  ActorFillingObserverTest() {
    InitAutofillClient();
    CreateAutofillDriver();
  }

 protected:
  using Future =
      base::test::TestFuture<base::expected<void, ActorFormFillingError>>;

  const AutofillProfile* AddProfile() {
    AutofillProfile profile = test::GetFullProfile();
    adm().AddProfile(profile);
    return adm().GetProfileByGUID(profile.guid());
  }

  AddressDataManager& adm() {
    return autofill_client().GetPersonalDataManager().address_data_manager();
  }

  // Returns the `credit_card_access_manager` of the `AutofillManager` with
  // this `index`.
  CreditCardAccessManager& credit_card_access_manager(size_t index = 0) {
    return *autofill_manager(index).GetCreditCardAccessManager();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_unit_test_environment_;
};

// Tests that the filling observer calls the callback immediately if an empty
// set of `field_ids` is passed.
TEST_F(ActorFillingObserverTest, EmptyInput) {
  Future future;

  ActorFillingObserver observer(autofill_client(), /*field_ids=*/{},
                                future.GetCallback());

  EXPECT_THAT(future.Get(), HasValue());
}

// Tests that the filling observer calls the callback with an error value if
// it is destroyed before the event is witnessed.
TEST_F(ActorFillingObserverTest, Destruction) {
  Future future;
  std::optional<ActorFillingObserver> observer;

  observer.emplace(autofill_client(),
                   /*field_ids=*/base::span_from_ref(MakeFieldGlobalId()),
                   future.GetCallback());
  observer.reset();

  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

// Tests that the filling observer calls the callback with success if a single
// field is filled.
TEST_F(ActorFillingObserverTest, SingleFieldFill) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId()};
  Future future;

  ActorFillingObserver observer(autofill_client(), field_ids,
                                future.GetCallback());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      mojom::ActionPersistence::kFill, field_ids, AddProfile());

  EXPECT_THAT(future.Get(), HasValue());
}

// Tests that previewing a field does not trigger the success callback.
TEST_F(ActorFillingObserverTest, SingleFieldPreview) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId()};
  Future future;
  std::optional<ActorFillingObserver> observer;

  observer.emplace(autofill_client(), field_ids, future.GetCallback());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      mojom::ActionPersistence::kPreview, field_ids, AddProfile());
  observer.reset();

  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

// Tests that the filling observer calls the callback with success after
// multiple fields are filled.
TEST_F(ActorFillingObserverTest, MultiFieldFill) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId(),
                                          MakeFieldGlobalId()};
  Future future;

  ActorFillingObserver observer(autofill_client(), field_ids,
                                future.GetCallback());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      mojom::ActionPersistence::kFill, std::vector({field_ids[0]}),
      AddProfile());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      mojom::ActionPersistence::kFill, std::vector({field_ids[1]}),
      AddProfile());

  EXPECT_THAT(future.Get(), HasValue());
}

// Tests that the filling observer calls the callback with an error if only
// some of the fields are filled.
TEST_F(ActorFillingObserverTest, IncompleteMultiFieldFill) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId(),
                                          MakeFieldGlobalId()};
  Future future;
  std::optional<ActorFillingObserver> observer;

  observer.emplace(autofill_client(), field_ids, future.GetCallback());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      mojom::ActionPersistence::kFill, std::vector({field_ids[0]}),
      AddProfile());
  observer.reset();

  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

// Tests that the filling observer times out after `GetFillingTimeout()` if no
// credit card fetch is ongoing.
TEST_F(ActorFillingObserverTest, FillingTimeout) {
  Future future;

  ActorFillingObserver observer(
      autofill_client(),
      /*field_ids=*/base::span_from_ref(MakeFieldGlobalId()),
      future.GetCallback());

  ASSERT_GT(ActorFillingObserver::GetFillingTimeout(), base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(ActorFillingObserver::GetFillingTimeout() -
                base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

// Tests that no timeout happens while a credit card fetch is ongoing. After
// fetch success/failure, the timeout is restarted.
TEST_F(ActorFillingObserverTest, FillingTimeoutWithCreditCardFetch) {
  CreateAutofillDriver();

  using Observer = CreditCardAccessManager::Observer;
  Future future;
  CreditCard card;
  ActorFillingObserver observer(
      autofill_client(),
      /*field_ids=*/base::span_from_ref(MakeFieldGlobalId()),
      future.GetCallback());

  ASSERT_GT(ActorFillingObserver::GetFillingTimeout(), base::Milliseconds(1));
  FastForwardBy(ActorFillingObserver::GetFillingTimeout() -
                base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());

  // The start of the fetch stops all timeouts.
  test_api(credit_card_access_manager(1))
      .NotifyObservers(&Observer::OnCreditCardFetchStarted, card);
  EXPECT_FALSE(future.IsReady());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());
  FastForwardBy(2 * ActorFillingObserver::GetFillingTimeout());
  EXPECT_FALSE(future.IsReady());

  // The fetch completion (successful or not) restarts the timer.
  test_api(credit_card_access_manager(1))
      .NotifyObservers(&Observer::OnCreditCardFetchFailed, &card);
  FastForwardBy(ActorFillingObserver::GetFillingTimeout() -
                base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

}  // namespace

}  // namespace autofill
