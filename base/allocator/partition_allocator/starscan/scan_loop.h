// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_SCAN_LOOP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_SCAN_LOOP_H_

#include <cstddef>
#include <cstdint>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/starscan/starscan_fwd.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_X86_64)
// Include order is important, so we disable formatting.
// clang-format off
// Including these headers directly should generally be avoided. For the
// scanning loop, we check at runtime which SIMD extension we can use. Since
// Chrome is compiled with -msse3 (the minimal requirement), we include the
// headers directly to make the intrinsics available. Another option could be to
// use inline assembly, but that would hinder compiler optimization for
// vectorized instructions.
#include <immintrin.h>
#include <smmintrin.h>
#include <avxintrin.h>
#include <avx2intrin.h>
// clang-format on
#endif

#if defined(PA_STARSCAN_NEON_SUPPORTED)
#include <arm_neon.h>
#endif

namespace base {
namespace internal {

// Iterates over range of memory using the best available SIMD extension.
// Assumes that 64bit platforms have cage support and the begin pointer of
// incoming ranges are properly aligned. The class is designed around the CRTP
// version of the "template method" (in GoF terms). CRTP is needed for fast
// static dispatch.
template <typename Derived>
class ScanLoop {
 public:
  explicit ScanLoop(SimdSupport simd_type) : simd_type_(simd_type) {}

  ScanLoop(const ScanLoop&) = delete;
  ScanLoop& operator=(const ScanLoop&) = delete;

  // Scan input range. Assumes the range is properly aligned.
  void Run(uintptr_t* begin, uintptr_t* end);

 private:
  const Derived& derived() const { return static_cast<const Derived&>(*this); }
  Derived& derived() { return static_cast<Derived&>(*this); }

#if defined(ARCH_CPU_X86_64)
  __attribute__((target("avx2"))) void RunAVX2(uintptr_t*, uintptr_t*);
  __attribute__((target("sse4.1"))) void RunSSE4(uintptr_t*, uintptr_t*);
#endif
#if defined(PA_STARSCAN_NEON_SUPPORTED)
  void RunNEON(uintptr_t*, uintptr_t*);
#endif

  void RunUnvectorized(uintptr_t*, uintptr_t*);

  SimdSupport simd_type_;
};

template <typename Derived>
void ScanLoop<Derived>::Run(uintptr_t* begin, uintptr_t* end) {
// We allow vectorization only for 64bit since they require support of the
// 64bit cage, and only for x86 because a special instruction set is required.
#if defined(ARCH_CPU_X86_64)
  if (simd_type_ == SimdSupport::kAVX2)
    return RunAVX2(begin, end);
  if (simd_type_ == SimdSupport::kSSE41)
    return RunSSE4(begin, end);
#elif defined(PA_STARSCAN_NEON_SUPPORTED)
  if (simd_type_ == SimdSupport::kNEON)
    return RunNEON(begin, end);
#endif  // defined(PA_STARSCAN_NEON_SUPPORTED)
  return RunUnvectorized(begin, end);
}

template <typename Derived>
void ScanLoop<Derived>::RunUnvectorized(uintptr_t* begin, uintptr_t* end) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % sizeof(uintptr_t)));
#if defined(PA_HAS_64_BITS_POINTERS)
  static constexpr uintptr_t mask = Derived::CageMask();
  const uintptr_t base = derived().CageBase();
#endif
  for (; begin < end; ++begin) {
    const uintptr_t maybe_ptr = *begin;
#if defined(PA_HAS_64_BITS_POINTERS)
    if (LIKELY((maybe_ptr & mask) != base))
      continue;
#else
    if (!maybe_ptr)
      continue;
#endif
    derived().CheckPointer(maybe_ptr);
  }
}

#if defined(ARCH_CPU_X86_64)
template <typename Derived>
__attribute__((target("avx2"))) void ScanLoop<Derived>::RunAVX2(
    uintptr_t* begin,
    uintptr_t* end) {
  static constexpr size_t kAlignmentRequirement = 32;
  static constexpr size_t kWordsInVector = 4;
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % kAlignmentRequirement));
  // Stick to integer instructions. This brings slightly better throughput. For
  // example, according to the Intel docs, on Broadwell and Haswell the CPI of
  // vmovdqa (_mm256_load_si256) is twice smaller (0.25) than that of vmovapd
  // (_mm256_load_pd).
  const __m256i vbase = _mm256_set1_epi64x(derived().CageBase());
  const __m256i cage_mask = _mm256_set1_epi64x(derived().CageMask());

  uintptr_t* payload = begin;
  for (; payload < (end - kWordsInVector); payload += kWordsInVector) {
    const __m256i maybe_ptrs =
        _mm256_load_si256(reinterpret_cast<__m256i*>(payload));
    const __m256i vand = _mm256_and_si256(maybe_ptrs, cage_mask);
    const __m256i vcmp = _mm256_cmpeq_epi64(vand, vbase);
    const int mask = _mm256_movemask_pd(_mm256_castsi256_pd(vcmp));
    if (LIKELY(!mask))
      continue;
    // It's important to extract pointers from the already loaded vector.
    // Otherwise, new loads can break in-cage assumption checked above.
    if (mask & 0b0001)
      derived().CheckPointer(_mm256_extract_epi64(maybe_ptrs, 0));
    if (mask & 0b0010)
      derived().CheckPointer(_mm256_extract_epi64(maybe_ptrs, 1));
    if (mask & 0b0100)
      derived().CheckPointer(_mm256_extract_epi64(maybe_ptrs, 2));
    if (mask & 0b1000)
      derived().CheckPointer(_mm256_extract_epi64(maybe_ptrs, 3));
  }
  RunUnvectorized(payload, end);
}

template <typename Derived>
__attribute__((target("sse4.1"))) void ScanLoop<Derived>::RunSSE4(
    uintptr_t* begin,
    uintptr_t* end) {
  static constexpr size_t kAlignmentRequirement = 16;
  static constexpr size_t kWordsInVector = 2;
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % kAlignmentRequirement));
  const __m128i vbase = _mm_set1_epi64x(derived().CageBase());
  const __m128i cage_mask = _mm_set1_epi64x(derived().CageMask());

  uintptr_t* payload = begin;
  for (; payload < (end - kWordsInVector); payload += kWordsInVector) {
    const __m128i maybe_ptrs =
        _mm_loadu_si128(reinterpret_cast<__m128i*>(payload));
    const __m128i vand = _mm_and_si128(maybe_ptrs, cage_mask);
    const __m128i vcmp = _mm_cmpeq_epi64(vand, vbase);
    const int mask = _mm_movemask_pd(_mm_castsi128_pd(vcmp));
    if (LIKELY(!mask))
      continue;
    // It's important to extract pointers from the already loaded vector.
    // Otherwise, new loads can break in-cage assumption checked above.
    if (mask & 0b01) {
      derived().CheckPointer(_mm_cvtsi128_si64(maybe_ptrs));
    }
    if (mask & 0b10) {
      // The mask is used to move the 4th and 3rd dwords into the second and
      // first position.
      static constexpr int kSecondWordMask = (3 << 2) | (2 << 0);
      const __m128i shuffled = _mm_shuffle_epi32(maybe_ptrs, kSecondWordMask);
      derived().CheckPointer(_mm_cvtsi128_si64(shuffled));
    }
  }
  RunUnvectorized(payload, end);
}
#endif  // defined(ARCH_CPU_X86_64)

#if defined(PA_STARSCAN_NEON_SUPPORTED)
template <typename Derived>
void ScanLoop<Derived>::RunNEON(uintptr_t* begin, uintptr_t* end) {
  static constexpr size_t kAlignmentRequirement = 16;
  static constexpr size_t kWordsInVector = 2;
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % kAlignmentRequirement));
  const uint64x2_t vbase = vdupq_n_u64(derived().CageBase());
  const uint64x2_t cage_mask = vdupq_n_u64(derived().CageMask());

  uintptr_t* payload = begin;
  for (; payload < (end - kWordsInVector); payload += kWordsInVector) {
    const uint64x2_t maybe_ptrs =
        vld1q_u64(reinterpret_cast<uint64_t*>(payload));
    const uint64x2_t vand = vandq_u64(maybe_ptrs, cage_mask);
    const uint64x2_t vcmp = vceqq_u64(vand, vbase);
    const uint32_t max = vmaxvq_u32(vreinterpretq_u32_u64(vcmp));
    if (LIKELY(!max))
      continue;
    // It's important to extract pointers from the already loaded vector.
    // Otherwise, new loads can break in-cage assumption checked above.
    if (vgetq_lane_u64(vcmp, 0))
      derived().CheckPointer(vgetq_lane_u64(maybe_ptrs, 0));
    if (vgetq_lane_u64(vcmp, 1))
      derived().CheckPointer(vgetq_lane_u64(maybe_ptrs, 1));
  }
  RunUnvectorized(payload, end);
}
#endif  // defined(PA_STARSCAN_NEON_SUPPORTED)

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_SCAN_LOOP_H_
