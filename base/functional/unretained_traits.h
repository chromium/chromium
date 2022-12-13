// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUNCTIONAL_UNRETAINED_TRAITS_H_
#define BASE_FUNCTIONAL_UNRETAINED_TRAITS_H_

#include "build/build_config.h"

#include <type_traits>

// Various opaque system types that should still be usable with the base
// callback system. Please keep sorted.
struct ANativeWindow;
struct DBusMessage;
struct HWND__;
struct VkBuffer_T;
struct VkDeviceMemory_T;
struct VkImage_T;
struct VkSemaphore_T;
struct VmaAllocation_T;
struct WGPUAdapterImpl;
struct fpdf_action_t__;
struct fpdf_annotation_t__;
struct fpdf_attachment_t__;
struct fpdf_bookmark_t__;
struct fpdf_document_t__;
struct fpdf_form_handle_t__;
struct fpdf_page_t__;
struct fpdf_structelement_t__;
struct hb_set_t;
struct wl_gpu;
struct wl_shm;
struct wl_surface;

namespace base::internal {

// True if `T` is completely defined or false otherwise. Note that this always
// returns false for function types.
template <typename T, typename = void>
inline constexpr bool IsCompleteTypeV = false;

template <typename T>
inline constexpr bool IsCompleteTypeV<T, std::void_t<decltype(sizeof(T))>> =
    true;

// Determining whether a type can be used with `Unretained()` requires that `T`
// be completely defined. Some system types have an intentionally opaque and
// incomplete representation, but should still be usable with `Unretained()`.
// The specializations here provide intentional escape hatches for those
// instances.
template <typename T>
inline constexpr bool IsIncompleteTypeSafeForUnretained = false;

// void* is occasionally used with callbacks; in the future, this may be more
// restricted/limited, but allow it for now.
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<void> = true;

// Functions have static lifetime and are always safe for use with
// `Unretained()`.
template <typename R, typename... Args>
inline constexpr bool IsIncompleteTypeSafeForUnretained<R(Args...)> = true;

// Various opaque system types that should still be usable with the base
// callback system. Please keep sorted.
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<ANativeWindow> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<DBusMessage> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<HWND__> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<VkBuffer_T> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<VkDeviceMemory_T> =
    true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<VkImage_T> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<VkSemaphore_T> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<VmaAllocation_T> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<WGPUAdapterImpl> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<fpdf_action_t__> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<fpdf_annotation_t__> =
    true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<fpdf_attachment_t__> =
    true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<fpdf_bookmark_t__> =
    true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<fpdf_document_t__> =
    true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<fpdf_form_handle_t__> =
    true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<fpdf_page_t__> = true;
template <>
inline constexpr bool
    IsIncompleteTypeSafeForUnretained<fpdf_structelement_t__> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<hb_set_t> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<wl_gpu> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<wl_shm> = true;
template <>
inline constexpr bool IsIncompleteTypeSafeForUnretained<wl_surface> = true;

template <typename T, typename SFINAE = void>
struct TypeSupportsUnretained {
// Incrementally enforce the requirement to be completely defined. For now,
// limit the failures to:
//
// - non-test code
// - non-official code (because these builds don't run as part of the default CQ
//   and are slower due to PGO and LTO)
// - Android, Linux or Windows
//
// to make this easier to land without potentially breaking the tree.
//
// TODO(https://crbug.com/1392872): Enable this on all platforms, then in
// official builds, and then in non-test code as well.
#if !defined(UNIT_TEST) && !defined(OFFICIAL_BUILD)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || \
    defined(FORCE_UNRETAINED_COMPLETENESS_CHECKS_FOR_TESTS)
  static_assert(IsCompleteTypeV<T> ||
                    IsIncompleteTypeSafeForUnretained<std::remove_cv_t<T>>,
                "T must be fully defined.");
#endif
#endif  // !defined(UNIT_TEST) && !defined(OFFICIAL_BUILD)

  static constexpr inline bool kValue = true;
};

// Matches against the marker tag created by the `DISALLOW_UNRETAINED()` macro
// in //base/functional/disallow_unretained.h.
template <typename T>
struct TypeSupportsUnretained<T, typename T::DisallowBaseUnretainedMarker> {
  static constexpr inline bool kValue = false;
};

// True if `T` is annotated with `DISALLOW_UNRETAINED()` and false otherwise.
template <typename T>
static inline constexpr bool TypeSupportsUnretainedV =
    TypeSupportsUnretained<T>::kValue;

}  // namespace base::internal

#endif  // BASE_FUNCTIONAL_UNRETAINED_TRAITS_H_
