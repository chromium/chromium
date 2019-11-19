// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/vector.h"

#include <windows.foundation.h>
#include <wrl/client.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ABI {
namespace Windows {
namespace Foundation {
namespace Collections {

// Note: As UWP does not provide int specializations for IObservableVector and
// VectorChangedEventHandler we need to supply our own. UUIDs were generated
// using `uuidgen`.
template <>
struct __declspec(uuid("21c2c195-91a4-4fce-8346-2a85f4478e26"))
    IObservableVector<int> : IObservableVector_impl<int> {};

template <>
struct __declspec(uuid("86b0071e-5e72-4d3d-82d3-420ebd2b2716"))
    VectorChangedEventHandler<int> : VectorChangedEventHandler_impl<int> {};

namespace {
using UriPtrAggregate = Internal::AggregateType<Uri*, IUriRuntimeClass*>;
}

template <>
struct __declspec(uuid("12311764-f245-4245-9dc9-bab258eddd4e"))
    IObservableVector<Uri*> : IObservableVector_impl<UriPtrAggregate> {};

template <>
struct __declspec(uuid("050e4b78-71b2-43ff-bf7c-f6ba589aced9"))
    VectorChangedEventHandler<Uri*>
    : VectorChangedEventHandler_impl<UriPtrAggregate> {};

}  // namespace Collections
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

namespace base {
namespace win {

namespace {

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::Uri;
using ABI::Windows::Foundation::Collections::CollectionChange;
using ABI::Windows::Foundation::Collections::CollectionChange_ItemChanged;
using ABI::Windows::Foundation::Collections::CollectionChange_ItemInserted;
using ABI::Windows::Foundation::Collections::CollectionChange_ItemRemoved;
using ABI::Windows::Foundation::Collections::CollectionChange_Reset;
using ABI::Windows::Foundation::Collections::IIterator;
using ABI::Windows::Foundation::Collections::IObservableVector;
using ABI::Windows::Foundation::Collections::IVectorChangedEventArgs;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::Collections::VectorChangedEventHandler;
using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::InhibitRoOriginateError;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

template <typename T>
class FakeVectorChangedEventHandler
    : public RuntimeClass<
          RuntimeClassFlags<ClassicCom | InhibitRoOriginateError>,
          VectorChangedEventHandler<T>> {
 public:
  explicit FakeVectorChangedEventHandler(ComPtr<IObservableVector<T>> vector)
      : vector_(std::move(vector)) {
    EXPECT_TRUE(SUCCEEDED(vector_->add_VectorChanged(this, &token_)));
  }

  ~FakeVectorChangedEventHandler() {
    EXPECT_TRUE(SUCCEEDED(vector_->remove_VectorChanged(token_)));
  }

  // VectorChangedEventHandler:
  IFACEMETHODIMP Invoke(IObservableVector<T>* sender,
                        IVectorChangedEventArgs* e) {
    sender_ = sender;
    EXPECT_TRUE(SUCCEEDED(e->get_CollectionChange(&change_)));
    EXPECT_TRUE(SUCCEEDED(e->get_Index(&index_)));
    return S_OK;
  }

  IObservableVector<T>* sender() { return sender_; }
  CollectionChange change() { return change_; }
  unsigned int index() { return index_; }

 private:
  ComPtr<IObservableVector<T>> vector_;
  EventRegistrationToken token_;
  IObservableVector<T>* sender_ = nullptr;
  CollectionChange change_ = CollectionChange_Reset;
  unsigned int index_ = 0;
};

// The ReplaceAll test requires a non-const data() member, thus these vectors
// are not declared const, even though no test mutates them.
std::vector<int> g_empty;
std::vector<int> g_one = {1};
std::vector<int> g_one_two = {1, 2};
std::vector<int> g_one_two_three = {1, 2, 3};

}  // namespace

TEST(VectorTest, GetAt_Empty) {
  auto vec = Make<Vector<int>>();
  int value;
  HRESULT hr = vec->GetAt(0, &value);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(VectorTest, GetAt_One) {
  auto vec = Make<Vector<int>>(g_one);
  int value;
  HRESULT hr = vec->GetAt(0, &value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1, value);

  hr = vec->GetAt(1, &value);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(VectorTest, GetAt_OneTwo) {
  auto vec = Make<Vector<int>>(g_one_two);
  int value;
  HRESULT hr = vec->GetAt(0, &value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1, value);

  hr = vec->GetAt(1, &value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2, value);

  hr = vec->GetAt(2, &value);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(VectorTest, GetAt_OneTwoThree) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  int value;
  HRESULT hr = vec->GetAt(0, &value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1, value);

  hr = vec->GetAt(1, &value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2, value);

  hr = vec->GetAt(2, &value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(3, value);

  hr = vec->GetAt(3, &value);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(VectorTest, get_Size_Empty) {
  auto vec = Make<Vector<int>>();
  unsigned size;
  HRESULT hr = vec->get_Size(&size);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, size);
}

TEST(VectorTest, get_Size_One) {
  auto vec = Make<Vector<int>>(g_one);
  unsigned size;
  HRESULT hr = vec->get_Size(&size);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1u, size);
}

TEST(VectorTest, get_Size_OneTwo) {
  auto vec = Make<Vector<int>>(g_one_two);
  unsigned size;
  HRESULT hr = vec->get_Size(&size);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2u, size);
}

TEST(VectorTest, get_Size_OneTwoThree) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  unsigned size;
  HRESULT hr = vec->get_Size(&size);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(3u, size);
}

TEST(VectorTest, GetView) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  ComPtr<IVectorView<int>> view;
  HRESULT hr = vec->GetView(&view);
  EXPECT_TRUE(SUCCEEDED(hr));

  int value;
  hr = view->GetAt(0, &value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1, value);

  hr = view->GetAt(1, &value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2, value);

  hr = view->GetAt(2, &value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(3, value);

  hr = view->GetAt(3, &value);
  EXPECT_EQ(E_BOUNDS, hr);

  unsigned size;
  hr = view->get_Size(&size);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(3u, size);

  unsigned index;
  boolean found;
  hr = view->IndexOf(1, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, index);
  EXPECT_TRUE(found);

  hr = view->IndexOf(2, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1u, index);
  EXPECT_TRUE(found);

  hr = view->IndexOf(3, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2u, index);
  EXPECT_TRUE(found);

  hr = view->IndexOf(4, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, index);
  EXPECT_FALSE(found);

  std::vector<int> copy(3);
  unsigned actual;
  hr = view->GetMany(0, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(3u, actual);
  EXPECT_THAT(copy, ElementsAre(1, 2, 3));

  hr = vec->Append(4);
  EXPECT_TRUE(SUCCEEDED(hr));

  // The view is supposed to be a snapshot of the vector when it's created.
  // Further modifications to the vector will invalidate the view.
  hr = view->GetAt(3, &value);
  EXPECT_EQ(E_CHANGED_STATE, hr);

  hr = view->get_Size(&size);
  EXPECT_EQ(E_CHANGED_STATE, hr);

  hr = view->IndexOf(1, &index, &found);
  EXPECT_EQ(E_CHANGED_STATE, hr);

  hr = view->GetMany(0, copy.size(), copy.data(), &actual);
  EXPECT_EQ(E_CHANGED_STATE, hr);
}

TEST(VectorTest, IndexOf_Empty) {
  auto vec = Make<Vector<int>>();
  unsigned index;
  boolean found;
  HRESULT hr = vec->IndexOf(1, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, index);
  EXPECT_FALSE(found);
}

TEST(VectorTest, IndexOf_One) {
  auto vec = Make<Vector<int>>(g_one);
  unsigned index;
  boolean found;

  HRESULT hr = vec->IndexOf(1, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, index);
  EXPECT_TRUE(found);

  hr = vec->IndexOf(2, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, index);
  EXPECT_FALSE(found);
}

TEST(VectorTest, IndexOf_OneTwo) {
  auto vec = Make<Vector<int>>(g_one_two);
  unsigned index;
  boolean found;

  HRESULT hr = vec->IndexOf(1, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, index);
  EXPECT_TRUE(found);

  hr = vec->IndexOf(2, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1u, index);
  EXPECT_TRUE(found);

  hr = vec->IndexOf(3, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, index);
  EXPECT_FALSE(found);
}

TEST(VectorTest, IndexOf_OneTwoThree) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  unsigned index;
  boolean found;

  HRESULT hr = vec->IndexOf(1, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, index);
  EXPECT_TRUE(found);

  hr = vec->IndexOf(2, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1u, index);
  EXPECT_TRUE(found);

  hr = vec->IndexOf(3, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2u, index);
  EXPECT_TRUE(found);

  hr = vec->IndexOf(4, &index, &found);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, index);
  EXPECT_FALSE(found);
}

TEST(VectorTest, SetAt) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  auto handler = Make<FakeVectorChangedEventHandler<int>>(vec.Get());

  HRESULT hr = vec->SetAt(0, 4);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(4, 2, 3));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemChanged, handler->change());
  EXPECT_EQ(0u, handler->index());

  hr = vec->SetAt(1, 5);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(4, 5, 3));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemChanged, handler->change());
  EXPECT_EQ(1u, handler->index());

  hr = vec->SetAt(2, 6);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(4, 5, 6));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemChanged, handler->change());
  EXPECT_EQ(2u, handler->index());

  hr = vec->SetAt(3, 7);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(VectorTest, InsertAt) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  auto handler = Make<FakeVectorChangedEventHandler<int>>(vec.Get());
  HRESULT hr = vec->InsertAt(4, 4);
  EXPECT_EQ(E_BOUNDS, hr);

  hr = vec->InsertAt(3, 4);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1, 2, 3, 4));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemInserted, handler->change());
  EXPECT_EQ(3u, handler->index());

  hr = vec->InsertAt(2, 5);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1, 2, 5, 3, 4));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemInserted, handler->change());
  EXPECT_EQ(2u, handler->index());

  hr = vec->InsertAt(1, 6);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1, 6, 2, 5, 3, 4));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemInserted, handler->change());
  EXPECT_EQ(1u, handler->index());

  hr = vec->InsertAt(0, 7);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(7, 1, 6, 2, 5, 3, 4));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemInserted, handler->change());
  EXPECT_EQ(0u, handler->index());
}

TEST(VectorTest, RemoveAt) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  auto handler = Make<FakeVectorChangedEventHandler<int>>(vec.Get());
  HRESULT hr = vec->RemoveAt(3);
  EXPECT_EQ(E_BOUNDS, hr);

  hr = vec->RemoveAt(2);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1, 2));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemRemoved, handler->change());
  EXPECT_EQ(2u, handler->index());

  hr = vec->RemoveAt(2);
  EXPECT_EQ(E_BOUNDS, hr);

  hr = vec->RemoveAt(1);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemRemoved, handler->change());
  EXPECT_EQ(1u, handler->index());

  hr = vec->RemoveAt(1);
  EXPECT_EQ(E_BOUNDS, hr);

  hr = vec->RemoveAt(0);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), IsEmpty());
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemRemoved, handler->change());
  EXPECT_EQ(0u, handler->index());

  hr = vec->RemoveAt(0);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(VectorTest, Append) {
  auto vec = Make<Vector<int>>();
  auto handler = Make<FakeVectorChangedEventHandler<int>>(vec.Get());
  HRESULT hr = vec->Append(1);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemInserted, handler->change());
  EXPECT_EQ(0u, handler->index());

  hr = vec->Append(2);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1, 2));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemInserted, handler->change());
  EXPECT_EQ(1u, handler->index());

  hr = vec->Append(3);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1, 2, 3));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemInserted, handler->change());
  EXPECT_EQ(2u, handler->index());
}

TEST(VectorTest, RemoveAtEnd) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  auto handler = Make<FakeVectorChangedEventHandler<int>>(vec.Get());
  HRESULT hr = vec->RemoveAtEnd();
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1, 2));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemRemoved, handler->change());
  EXPECT_EQ(2u, handler->index());

  hr = vec->RemoveAtEnd();
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemRemoved, handler->change());
  EXPECT_EQ(1u, handler->index());

  hr = vec->RemoveAtEnd();
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), IsEmpty());
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemRemoved, handler->change());
  EXPECT_EQ(0u, handler->index());

  hr = vec->RemoveAtEnd();
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(VectorTest, Clear) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  auto handler = Make<FakeVectorChangedEventHandler<int>>(vec.Get());
  HRESULT hr = vec->Clear();
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), IsEmpty());
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_Reset, handler->change());
  EXPECT_EQ(0u, handler->index());
}

TEST(VectorTest, GetMany) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  std::vector<int> copy;
  unsigned actual;
  HRESULT hr = vec->GetMany(0, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, actual);
  EXPECT_THAT(copy, IsEmpty());

  copy.resize(1);
  hr = vec->GetMany(0, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1u, actual);
  EXPECT_THAT(copy, ElementsAre(1));

  copy.resize(2);
  hr = vec->GetMany(0, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2u, actual);
  EXPECT_THAT(copy, ElementsAre(1, 2));

  copy.resize(3);
  hr = vec->GetMany(0, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(3u, actual);
  EXPECT_THAT(copy, ElementsAre(1, 2, 3));

  copy.resize(4);
  hr = vec->GetMany(0, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(3u, actual);
  EXPECT_THAT(copy, ElementsAre(1, 2, 3, 0));

  copy.resize(0);
  hr = vec->GetMany(1, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, actual);
  EXPECT_THAT(copy, IsEmpty());

  copy.resize(1);
  hr = vec->GetMany(1, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1u, actual);
  EXPECT_THAT(copy, ElementsAre(2));

  copy.resize(2);
  hr = vec->GetMany(1, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2u, actual);
  EXPECT_THAT(copy, ElementsAre(2, 3));

  copy.resize(3);
  hr = vec->GetMany(1, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2u, actual);
  EXPECT_THAT(copy, ElementsAre(2, 3, 0));

  copy.resize(0);
  hr = vec->GetMany(2, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, actual);
  EXPECT_THAT(copy, IsEmpty());

  copy.resize(1);
  hr = vec->GetMany(2, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1u, actual);
  EXPECT_THAT(copy, ElementsAre(3));

  copy.resize(2);
  hr = vec->GetMany(2, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1u, actual);
  EXPECT_THAT(copy, ElementsAre(3, 0));

  hr = vec->GetMany(3, copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, actual);

  hr = vec->GetMany(4, copy.size(), copy.data(), &actual);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(VectorTest, ReplaceAll) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  auto handler = Make<FakeVectorChangedEventHandler<int>>(vec.Get());
  HRESULT hr = vec->ReplaceAll(g_empty.size(), g_empty.data());
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), IsEmpty());
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_Reset, handler->change());
  EXPECT_EQ(0u, handler->index());

  hr = vec->ReplaceAll(g_one.size(), g_one.data());
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_Reset, handler->change());
  EXPECT_EQ(0u, handler->index());

  hr = vec->ReplaceAll(g_one_two.size(), g_one_two.data());
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1, 2));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_Reset, handler->change());
  EXPECT_EQ(0u, handler->index());

  hr = vec->ReplaceAll(g_one_two_three.size(), g_one_two_three.data());
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_THAT(vec->vector_for_testing(), ElementsAre(1, 2, 3));
  EXPECT_EQ(vec.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_Reset, handler->change());
  EXPECT_EQ(0u, handler->index());
}

// Uri* is an AggregateType which ABI representation is IUriRuntimeClass*.
TEST(VectorTest, ConstructWithAggregateType) {
  auto vec = Make<Vector<Uri*>>();
  unsigned size;
  HRESULT hr = vec->get_Size(&size);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(0u, size);
}

TEST(VectorTest, First) {
  auto vec = Make<Vector<int>>(g_one_two_three);
  ComPtr<IIterator<int>> iterator;
  HRESULT hr = vec->First(&iterator);
  EXPECT_TRUE(SUCCEEDED(hr));
  boolean has_current;
  hr = iterator->get_HasCurrent(&has_current);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_TRUE(has_current);
  int current;
  hr = iterator->get_Current(&current);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(1, current);
  hr = iterator->MoveNext(&has_current);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_TRUE(has_current);
  hr = iterator->get_Current(&current);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(2, current);
  hr = iterator->MoveNext(&has_current);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_TRUE(has_current);
  hr = iterator->get_Current(&current);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(3, current);
  hr = iterator->MoveNext(&has_current);
  EXPECT_FALSE(SUCCEEDED(hr));
  EXPECT_EQ(E_BOUNDS, hr);
  hr = iterator->get_Current(&current);
  EXPECT_FALSE(SUCCEEDED(hr));
  EXPECT_EQ(E_BOUNDS, hr);

  hr = vec->First(&iterator);
  EXPECT_TRUE(SUCCEEDED(hr));
  std::vector<int> copy(3);
  unsigned actual;
  hr = iterator->GetMany(copy.size(), copy.data(), &actual);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(3u, actual);
  EXPECT_THAT(copy, ElementsAre(1, 2, 3));
}

}  // namespace win
}  // namespace base
