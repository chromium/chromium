// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/map.h"

#include <windows.foundation.h>

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ABI::Windows::Foundation::Collections {

// Add missing template specializations (since UWP doesn't provide them):

// Map<int, double> specializations:
template <>
struct __declspec(uuid("34784dd6-b37b-4680-b391-899be4f755b6"))
    IKeyValuePair<int, double> : IKeyValuePair_impl<int, double> {};

template <>
struct __declspec(uuid("c00bd9bd-cce5-46d6-9dc7-f03067e6d523"))
    IMap<int, double> : IMap_impl<int, double> {};

template <>
struct __declspec(uuid("30e075af-9ba2-4562-9f10-a13a0e57ca5b"))
    IMapView<int, double> : IMapView_impl<int, double> {};

template <>
struct __declspec(uuid("0a0e8ed6-7deb-4fd4-8033-38d270c69301"))
    IObservableMap<int, double> : IObservableMap_impl<int, double> {};

template <>
struct __declspec(uuid("f41f9179-9c95-4755-af55-929a250fc0aa"))
    IMapChangedEventArgs<int> : IMapChangedEventArgs_impl<int> {};

template <>
struct __declspec(uuid("79196029-07f6-47c6-9933-9ac3a04e7731"))
    MapChangedEventHandler<int, double>
    : MapChangedEventHandler_impl<int, double> {};

template <>
struct __declspec(uuid("bfd254c3-5ede-4f8f-9e48-3636347f6fe0"))
    IIterable<IKeyValuePair<int, double>*>
    : IIterable_impl<IKeyValuePair<int, double>*> {};

template <>
struct __declspec(uuid("6bb5c7ff-964e-469f-87d3-42daaea8e58d"))
    IIterator<IKeyValuePair<int, double>*>
    : IIterator_impl<IKeyValuePair<int, double>*> {};

template <>
struct __declspec(uuid("7d27014c-8df7-4977-bf98-b0c821f5f988"))
    IVector<IKeyValuePair<int, double>*>
    : IVector_impl<IKeyValuePair<int, double>*> {};

template <>
struct __declspec(uuid("d33b7a5c-9da6-4a6a-8b2e-e08cc0240d77"))
    IVectorView<IKeyValuePair<int, double>*>
    : IVectorView_impl<IKeyValuePair<int, double>*> {};

template <>
struct __declspec(uuid("e5b0d7f2-915d-4831-9a04-466fed63cfa0"))
    VectorChangedEventHandler<IKeyValuePair<int, double>*>
    : VectorChangedEventHandler_impl<IKeyValuePair<int, double>*> {};

template <>
struct __declspec(uuid("27c3ee04-457f-42dd-9556-8f7c4994d7af"))
    IObservableVector<IKeyValuePair<int, double>*>
    : IObservableVector_impl<IKeyValuePair<int, double>*> {};

// Map<Uri*, Uri*> specializations:
template <>
struct __declspec(uuid("c03984bc-b800-43e4-a36e-3c8c4a34c005")) IMap<Uri*, Uri*>
    : IMap_impl<Uri*, Uri*> {};

template <>
struct __declspec(uuid("93ec9c52-1b0b-4fd8-ab5a-f6ea32db0e35"))
    IMapView<Uri*, Uri*> : IMapView_impl<Uri*, Uri*> {};

template <>
struct __declspec(uuid("9b711c83-5f01-4604-9e01-3d586b3f9cdd"))
    IObservableMap<Uri*, Uri*> : IObservableMap_impl<Uri*, Uri*> {};

template <>
struct __declspec(uuid("f41f9179-9c95-4755-af55-929a250fc0aa"))
    IMapChangedEventArgs<Uri*> : IMapChangedEventArgs_impl<Uri*> {};

template <>
struct __declspec(uuid("6d758124-f99a-47e7-ab74-7cff7359b206"))
    MapChangedEventHandler<Uri*, Uri*>
    : MapChangedEventHandler_impl<Uri*, Uri*> {};

template <>
struct __declspec(uuid("8b270b8a-d74b-459b-9933-81cb234d7c5e"))
    IKeyValuePair<Uri*, Uri*> : IKeyValuePair_impl<Uri*, Uri*> {};

template <>
struct __declspec(uuid("6368bcea-dfbc-4847-ba50-9e217fc2d5c3"))
    IIterable<IKeyValuePair<Uri*, Uri*>*>
    : IIterable_impl<IKeyValuePair<Uri*, Uri*>*> {};

template <>
struct __declspec(uuid("7653cf9f-9d0b-46d3-882e-4c0afb209333"))
    IIterator<IKeyValuePair<Uri*, Uri*>*>
    : IIterator_impl<IKeyValuePair<Uri*, Uri*>*> {};

template <>
struct __declspec(uuid("98c3f5a7-237d-494b-ba89-4a49368d5491"))
    IVector<IKeyValuePair<Uri*, Uri*>*>
    : IVector_impl<IKeyValuePair<Uri*, Uri*>*> {};

template <>
struct __declspec(uuid("2cfc2617-7c88-4482-8158-97bf7cc458d7"))
    IVectorView<IKeyValuePair<Uri*, Uri*>*>
    : IVectorView_impl<IKeyValuePair<Uri*, Uri*>*> {};

template <>
struct __declspec(uuid("bb581e03-3ee7-4c01-8035-4f581c5e91f5"))
    VectorChangedEventHandler<IKeyValuePair<Uri*, Uri*>*>
    : VectorChangedEventHandler_impl<IKeyValuePair<Uri*, Uri*>*> {};

template <>
struct __declspec(uuid("fb0bd692-34c3-4242-a085-58ed71e8ea6b"))
    IObservableVector<IKeyValuePair<Uri*, Uri*>*>
    : IObservableVector_impl<IKeyValuePair<Uri*, Uri*>*> {};

// Map<HSTRING*, IInspectable*> specializations:
template <>
struct __declspec(uuid("c6682be1-963c-4101-85aa-63db583eb0d5"))
    IVector<IKeyValuePair<HSTRING, IInspectable*>*>
    : IVector_impl<IKeyValuePair<HSTRING, IInspectable*>*> {};

template <>
struct __declspec(uuid("868e5342-49c8-478f-af0f-1691e1bbbb7c"))
    IVectorView<IKeyValuePair<HSTRING, IInspectable*>*>
    : IVectorView_impl<IKeyValuePair<HSTRING, IInspectable*>*> {};

template <>
struct __declspec(uuid("cd99b82f-a768-405f-9123-be509146fef8"))
    VectorChangedEventHandler<IKeyValuePair<HSTRING, IInspectable*>*>
    : VectorChangedEventHandler_impl<IKeyValuePair<HSTRING, IInspectable*>*> {};

template <>
struct __declspec(uuid("079e2180-0c7a-4508-85ff-7a5f2b29b92b"))
    IObservableVector<IKeyValuePair<HSTRING, IInspectable*>*>
    : IObservableVector_impl<IKeyValuePair<HSTRING, IInspectable*>*> {};

}  // namespace ABI::Windows::Foundation::Collections

namespace base::win {

namespace {

using ABI::Windows::Foundation::IPropertyValue;
using ABI::Windows::Foundation::IPropertyValueStatics;
using ABI::Windows::Foundation::Uri;
using ABI::Windows::Foundation::Collections::CollectionChange;
using ABI::Windows::Foundation::Collections::CollectionChange_ItemChanged;
using ABI::Windows::Foundation::Collections::CollectionChange_ItemInserted;
using ABI::Windows::Foundation::Collections::CollectionChange_ItemRemoved;
using ABI::Windows::Foundation::Collections::CollectionChange_Reset;
using ABI::Windows::Foundation::Collections::IIterator;
using ABI::Windows::Foundation::Collections::IKeyValuePair;
using ABI::Windows::Foundation::Collections::IMapChangedEventArgs;
using ABI::Windows::Foundation::Collections::IMapView;
using ABI::Windows::Foundation::Collections::IObservableMap;
using ABI::Windows::Foundation::Collections::MapChangedEventHandler;
using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::InhibitRoOriginateError;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

const wchar_t kTestKey[] = L"Test key";
const wchar_t kTestValue[] = L"Test value";

const std::map<int, double, internal::Less> g_one{{1, 10.7}};
const std::map<int, double, internal::Less> g_two{{1, 10.7}, {2, 20.3}};

HRESULT GetPropertyValueStaticsActivationFactory(
    IPropertyValueStatics** statics) {
  return base::win::GetActivationFactory<
      IPropertyValueStatics, RuntimeClass_Windows_Foundation_PropertyValue>(
      statics);
}

template <typename K, typename V>
class FakeMapChangedEventHandler
    : public RuntimeClass<
          RuntimeClassFlags<ClassicCom | InhibitRoOriginateError>,
          MapChangedEventHandler<K, V>> {
 public:
  explicit FakeMapChangedEventHandler(ComPtr<IObservableMap<K, V>> map)
      : map_(std::move(map)) {
    EXPECT_HRESULT_SUCCEEDED(map_->add_MapChanged(this, &token_));
  }

  ~FakeMapChangedEventHandler() override {
    EXPECT_HRESULT_SUCCEEDED(map_->remove_MapChanged(token_));
  }

  // MapChangedEventHandler:
  IFACEMETHODIMP Invoke(IObservableMap<K, V>* sender,
                        IMapChangedEventArgs<K>* e) {
    sender_ = sender;
    EXPECT_HRESULT_SUCCEEDED(e->get_CollectionChange(&change_));
    EXPECT_HRESULT_SUCCEEDED(e->get_Key(&key_));
    return S_OK;
  }

  IObservableMap<K, V>* sender() { return sender_; }
  CollectionChange change() { return change_; }
  K key() const { return key_; }

 private:
  ComPtr<IObservableMap<K, V>> map_;
  EventRegistrationToken token_;
  raw_ptr<IObservableMap<K, V>> sender_ = nullptr;
  CollectionChange change_ = CollectionChange_Reset;
  K key_ = 0;
};

}  // namespace

TEST(MapTest, Lookup_Empty) {
  auto map = Make<Map<int, double>>();
  double value;
  HRESULT hr = map->Lookup(1, &value);
  EXPECT_EQ(E_BOUNDS, hr);
  hr = map->Lookup(2, &value);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(MapTest, Lookup_One) {
  auto map = Make<Map<int, double>>(g_one);
  double value;
  HRESULT hr = map->Lookup(1, &value);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(10.7, value);
  hr = map->Lookup(2, &value);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(MapTest, Lookup_Two) {
  auto map = Make<Map<int, double>>(g_two);
  double value;
  HRESULT hr = map->Lookup(1, &value);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(10.7, value);
  hr = map->Lookup(2, &value);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(20.3, value);
}

TEST(MapTest, get_Size_Empty) {
  auto map = Make<Map<int, double>>();
  unsigned int size;
  HRESULT hr = map->get_Size(&size);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(0u, size);
}

TEST(MapTest, get_Size_One) {
  auto map = Make<Map<int, double>>(g_one);
  unsigned int size;
  HRESULT hr = map->get_Size(&size);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(1u, size);
}

TEST(MapTest, get_Size_Two) {
  auto map = Make<Map<int, double>>(g_two);
  unsigned int size;
  HRESULT hr = map->get_Size(&size);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(2u, size);
}

TEST(MapTest, HasKey_Empty) {
  auto map = Make<Map<int, double>>();
  boolean found;
  HRESULT hr = map->HasKey(1, &found);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_FALSE(found);
}

TEST(MapTest, HasKey_One) {
  auto map = Make<Map<int, double>>(g_one);
  boolean found;
  HRESULT hr = map->HasKey(1, &found);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(found);
  hr = map->HasKey(2, &found);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_FALSE(found);
}

TEST(MapTest, HasKey_Two) {
  auto map = Make<Map<int, double>>(g_two);
  boolean found;
  HRESULT hr = map->HasKey(1, &found);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(found);
  hr = map->HasKey(2, &found);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(found);
}

TEST(MapTest, GetView) {
  auto map = Make<Map<int, double>>(g_two);
  ComPtr<IMapView<int, double>> view;
  HRESULT hr = map->GetView(&view);
  EXPECT_HRESULT_SUCCEEDED(hr);

  double value;
  hr = view->Lookup(1, &value);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(10.7, value);
  hr = view->Lookup(2, &value);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(20.3, value);

  unsigned int size;
  hr = view->get_Size(&size);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(2u, size);

  boolean found;
  hr = view->HasKey(1, &found);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(found);
  hr = view->HasKey(2, &found);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(found);

  // The view is supposed to be a snapshot of the map when it's created.
  // Further modifications to the map will invalidate the view.
  boolean replaced;
  hr = map->Insert(3, 11.2, &replaced);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_FALSE(replaced);

  hr = view->Lookup(1, &value);
  EXPECT_EQ(E_CHANGED_STATE, hr);

  hr = view->get_Size(&size);
  EXPECT_EQ(E_CHANGED_STATE, hr);

  hr = view->HasKey(1, &found);
  EXPECT_EQ(E_CHANGED_STATE, hr);
}

TEST(MapTest, Insert_Empty) {
  auto map = Make<Map<int, double>>();
  auto handler = Make<FakeMapChangedEventHandler<int, double>>(map.Get());
  boolean replaced;
  HRESULT hr = map->Insert(1, 11.2, &replaced);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_FALSE(replaced);
  EXPECT_EQ(map.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemInserted, handler->change());
  EXPECT_EQ(1, handler->key());
  double value;
  hr = map->Lookup(1, &value);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(11.2, value);
}

TEST(MapTest, Insert_One) {
  auto map = Make<Map<int, double>>(g_one);
  auto handler = Make<FakeMapChangedEventHandler<int, double>>(map.Get());
  double value;
  HRESULT hr = map->Lookup(1, &value);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(10.7, value);
  boolean replaced;
  hr = map->Insert(1, 11.2, &replaced);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(replaced);
  EXPECT_EQ(map.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemChanged, handler->change());
  EXPECT_EQ(1, handler->key());
  hr = map->Lookup(1, &value);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(11.2, value);
}

TEST(MapTest, Remove_One) {
  auto map = Make<Map<int, double>>(g_one);
  auto handler = Make<FakeMapChangedEventHandler<int, double>>(map.Get());
  double value;
  HRESULT hr = map->Lookup(1, &value);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(10.7, value);
  hr = map->Remove(1);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(map.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_ItemRemoved, handler->change());
  EXPECT_EQ(1, handler->key());
  hr = map->Lookup(1, &value);
  EXPECT_EQ(E_BOUNDS, hr);
}

TEST(MapTest, Clear) {
  auto map = Make<Map<int, double>>(g_one);
  auto handler = Make<FakeMapChangedEventHandler<int, double>>(map.Get());
  HRESULT hr = map->Clear();
  EXPECT_EQ(map.Get(), handler->sender());
  EXPECT_EQ(CollectionChange_Reset, handler->change());
  EXPECT_EQ(0, handler->key());
  unsigned int size;
  hr = map->get_Size(&size);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(0u, size);
}

// Uri* is an AggregateType which ABI representation is IUriRuntimeClass*.
TEST(MapTest, ConstructWithAggregateTypes) {
  auto map = Make<Map<Uri*, Uri*>>();
  unsigned size;
  HRESULT hr = map->get_Size(&size);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(0u, size);
}

TEST(MapTest, First) {
  auto map = Make<Map<int, double>>(g_two);
  ComPtr<IIterator<IKeyValuePair<int, double>*>> iterator;

  // Test iteration.
  HRESULT hr = map->First(&iterator);
  EXPECT_HRESULT_SUCCEEDED(hr);
  boolean has_current;
  hr = iterator->get_HasCurrent(&has_current);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(has_current);
  ComPtr<IKeyValuePair<int, double>> current;
  hr = iterator->get_Current(&current);
  EXPECT_HRESULT_SUCCEEDED(hr);
  int key;
  hr = current->get_Key(&key);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(1, key);
  double value;
  hr = current->get_Value(&value);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(10.7, value);
  hr = iterator->MoveNext(&has_current);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(has_current);
  hr = iterator->get_Current(&current);
  EXPECT_HRESULT_SUCCEEDED(hr);
  hr = current->get_Key(&key);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(2, key);
  hr = current->get_Value(&value);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(20.3, value);
  hr = iterator->MoveNext(&has_current);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_FALSE(has_current);
  hr = iterator->get_Current(&current);
  EXPECT_FALSE(SUCCEEDED(hr));
  EXPECT_EQ(E_BOUNDS, hr);
  hr = iterator->MoveNext(&has_current);
  EXPECT_FALSE(SUCCEEDED(hr));
  EXPECT_EQ(E_BOUNDS, hr);
  EXPECT_FALSE(has_current);

  // Test invalidation.
  hr = map->First(&iterator);
  EXPECT_HRESULT_SUCCEEDED(hr);
  hr = iterator->get_HasCurrent(&has_current);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(has_current);
  boolean replaced;
  hr = map->Insert(3, 11.2, &replaced);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_FALSE(replaced);
  hr = iterator->get_HasCurrent(&has_current);
  EXPECT_EQ(E_CHANGED_STATE, hr);
  hr = iterator->MoveNext(&has_current);
  EXPECT_EQ(E_CHANGED_STATE, hr);
}

TEST(MapTest, Properties) {
  // This test case validates Map against Windows property key system,
  // which is used to store WinRT device properties.
  ScopedWinrtInitializer winrt_initializer;
  ASSERT_TRUE(winrt_initializer.Succeeded());

  auto map = Make<Map<HSTRING, IInspectable*>>();

  ComPtr<IPropertyValueStatics> property_value_statics;
  HRESULT hr =
      GetPropertyValueStaticsActivationFactory(&property_value_statics);
  EXPECT_HRESULT_SUCCEEDED(hr);

  base::win::HStringReference value_stringref_inserted(kTestValue);
  ComPtr<IPropertyValue> value_inserted;
  hr = property_value_statics->CreateString(value_stringref_inserted.Get(),
                                            &value_inserted);
  EXPECT_HRESULT_SUCCEEDED(hr);

  base::win::HStringReference key_stringref_inserted(kTestKey);
  boolean replaced;
  hr = map->Insert(key_stringref_inserted.Get(), value_inserted.Get(),
                   &replaced);
  EXPECT_HRESULT_SUCCEEDED(hr);

  base::win::HStringReference key_stringref_lookedup(kTestKey);
  ComPtr<IInspectable> value_inspectable_lookedup;
  hr = map->Lookup(key_stringref_lookedup.Get(), &value_inspectable_lookedup);
  EXPECT_HRESULT_SUCCEEDED(hr);

  ComPtr<IPropertyValue> value_lookedup;
  hr = value_inspectable_lookedup.As(&value_lookedup);
  EXPECT_HRESULT_SUCCEEDED(hr);

  HSTRING value_string_lookedup;
  hr = value_lookedup->GetString(&value_string_lookedup);
  EXPECT_HRESULT_SUCCEEDED(hr);

  auto value_stringref_lookedup = ScopedHString(value_string_lookedup);
  EXPECT_EQ(kTestValue, value_stringref_lookedup.Get());
}

}  // namespace base::win
