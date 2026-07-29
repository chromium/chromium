// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LOCALE_HOLDER_H_
#define BASE_I18N_LOCALE_HOLDER_H_

#include <type_traits>

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_tag.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace base::i18n {

template <typename T>
class ScopedLocaleOverride;

// ThreadSafeLocaleHolder is a thread-safe container for a single LanguageTag
// (locale). It allows any thread to safely read and write the active locale
// concurrently under an internal lock. This class is final and non-virtual.
class BASE_I18N_EXPORT ThreadSafeLocaleHolder final {
 public:
  explicit ThreadSafeLocaleHolder(LanguageTag initial_locale);
  ~ThreadSafeLocaleHolder();

  // Returns the current LanguageTag. Safe to call concurrently from any thread.
  LanguageTag GetLocale() const;

  // Updates the current LanguageTag. Safe to call concurrently from any thread.
  void SetLocale(const LanguageTag& locale);

 private:
  template <typename T>
  friend class ScopedLocaleOverride;

  mutable base::Lock lock_;
  LanguageTag locale_ GUARDED_BY(lock_);
};

// SequenceCheckedLocaleHolder is a locale container that restricts write
// operations (calls to SetLocale) to a single sequence (via SEQUENCE_CHECKER).
// However, it supports thread-safe GetLocale(), allowing multiple threads
// and sequences to safely and concurrently read the active locale.
class BASE_I18N_EXPORT SequenceCheckedLocaleHolder final {
 public:
  explicit SequenceCheckedLocaleHolder(LanguageTag initial_locale);
  ~SequenceCheckedLocaleHolder();

  // Returns the current LanguageTag. Safe to call concurrently from any thread.
  LanguageTag GetLocale() const;

  // Updates the current LanguageTag. Must only be called on the sequence that
  // this holder is bound to.
  void SetLocale(const LanguageTag& locale);

 private:
  template <typename T>
  friend class ScopedLocaleOverride;
  SEQUENCE_CHECKER(sequence_checker_);
  ThreadSafeLocaleHolder holder_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_LOCALE_HOLDER_H_
