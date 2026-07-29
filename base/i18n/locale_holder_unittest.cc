// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/locale_holder.h"

#include <vector>

#include "base/i18n/language_tag.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {
namespace {

TEST(ThreadSafeLocaleHolderTest, GetAndSet) {
  LanguageTag initial_locale = GetKnownLanguageTag("en-US");
  ThreadSafeLocaleHolder holder(initial_locale);

  // Verify that the initial locale is correctly returned.
  EXPECT_EQ(holder.GetLocale(), initial_locale);

  // Verify that setting a new locale updates the value.
  LanguageTag new_locale = GetKnownLanguageTag("fr-FR");
  holder.SetLocale(new_locale);
  EXPECT_EQ(holder.GetLocale(), new_locale);
}

TEST(ThreadSafeLocaleHolderTest, NoOpSet) {
  LanguageTag locale = GetKnownLanguageTag("en-US");
  ThreadSafeLocaleHolder holder(locale);

  // Setting the same locale shouldn't change the value or cause any issue.
  holder.SetLocale(locale);
  EXPECT_EQ(holder.GetLocale(), locale);
}

template <typename T>
class LocaleReaderThread : public base::SimpleThread {
 public:
  LocaleReaderThread(T& holder, int iterations)
      : SimpleThread("LocaleReaderThread"),
        holder_(holder),
        iterations_(iterations) {}

  void Run() override {
    LanguageTag en_us = GetKnownLanguageTag("en-US");
    LanguageTag fr_fr = GetKnownLanguageTag("fr-FR");
    for (int i = 0; i < iterations_; ++i) {
      LanguageTag tag = holder_->GetLocale();
      EXPECT_TRUE(tag == en_us || tag == fr_fr);
    }
  }

 private:
  const raw_ref<T> holder_;
  const int iterations_;
};

class LocaleWriterThread : public base::SimpleThread {
 public:
  LocaleWriterThread(ThreadSafeLocaleHolder& holder, int iterations)
      : SimpleThread("LocaleWriterThread"),
        holder_(holder),
        iterations_(iterations) {}

  void Run() override {
    LanguageTag en_us = GetKnownLanguageTag("en-US");
    LanguageTag fr_fr = GetKnownLanguageTag("fr-FR");
    for (int i = 0; i < iterations_; ++i) {
      if (i % 2 == 0) {
        holder_->SetLocale(en_us);
      } else {
        holder_->SetLocale(fr_fr);
      }
    }
  }

 private:
  const raw_ref<ThreadSafeLocaleHolder> holder_;
  const int iterations_;
};

TEST(ThreadSafeLocaleHolderTest, ThreadSafety) {
  ThreadSafeLocaleHolder holder(GetKnownLanguageTag("en-US"));

  const int kIterations = 1000;
  const int kReaderThreadsCount = 4;
  const int kWriterThreadsCount = 4;

  std::vector<std::unique_ptr<LocaleReaderThread<ThreadSafeLocaleHolder>>>
      readers;
  std::vector<std::unique_ptr<LocaleWriterThread>> writers;

  for (int i = 0; i < kReaderThreadsCount; ++i) {
    readers.push_back(
        std::make_unique<LocaleReaderThread<ThreadSafeLocaleHolder>>(
            holder, kIterations));
  }
  for (int i = 0; i < kWriterThreadsCount; ++i) {
    writers.push_back(
        std::make_unique<LocaleWriterThread>(holder, kIterations));
  }

  // Start all threads.
  for (auto& r : readers) {
    r->Start();
  }
  for (auto& w : writers) {
    w->Start();
  }

  // Wait for all threads to finish.
  for (auto& r : readers) {
    r->Join();
  }
  for (auto& w : writers) {
    w->Join();
  }
}

TEST(SequenceCheckedLocaleHolderTest, GetAndSet) {
  LanguageTag initial_locale = GetKnownLanguageTag("en-US");
  SequenceCheckedLocaleHolder holder(initial_locale);

  // Verify that the initial locale is correctly returned.
  EXPECT_EQ(holder.GetLocale(), initial_locale);

  // Verify that setting a new locale updates the value.
  LanguageTag new_locale = GetKnownLanguageTag("fr-FR");
  holder.SetLocale(new_locale);
  EXPECT_EQ(holder.GetLocale(), new_locale);
}

TEST(SequenceCheckedLocaleHolderTest, SequenceValidation) {
  base::test::TaskEnvironment task_environment;
  SequenceCheckedLocaleHolder holder(GetKnownLanguageTag("en-US"));

  // Bind the sequence checker to the current sequence (main thread/task runner
  // sequence).
  holder.SetLocale(GetKnownLanguageTag("en-US"));

  // Accessing GetLocale() from a different sequence should succeed!
  // Accessing SetLocale() from a different sequenced task runner should
  // trigger a DCHECK/check failure if DCHECKs are enabled.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  base::RunLoop run_loop;
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](SequenceCheckedLocaleHolder* h, base::OnceClosure quit) {
            // GetLocale() can be read from any thread or sequence.
            EXPECT_EQ(h->GetLocale(), GetKnownLanguageTag("en-US"));

#if DCHECK_IS_ON()
            EXPECT_DCHECK_DEATH(
                { h->SetLocale(GetKnownLanguageTag("fr-FR")); });
#else
            // In non-DCHECK builds, SetLocale happily succeeds.
            h->SetLocale(GetKnownLanguageTag("fr-FR"));
            EXPECT_EQ(h->GetLocale(), GetKnownLanguageTag("fr-FR"));
#endif
            std::move(quit).Run();
          },
          base::Unretained(&holder), run_loop.QuitClosure()));
  run_loop.Run();
}

TEST(SequenceCheckedLocaleHolderTest, ConcurrentReads) {
  SequenceCheckedLocaleHolder holder(GetKnownLanguageTag("en-US"));

  // Bind the sequence checker by writing.
  holder.SetLocale(GetKnownLanguageTag("en-US"));

  const int kIterations = 1000;
  const int kReaderThreadsCount = 4;

  std::vector<std::unique_ptr<LocaleReaderThread<SequenceCheckedLocaleHolder>>>
      readers;
  for (int i = 0; i < kReaderThreadsCount; ++i) {
    readers.push_back(
        std::make_unique<LocaleReaderThread<SequenceCheckedLocaleHolder>>(
            holder, kIterations));
  }

  // Start all reader threads. Since GetLocale doesn't have sequence checks,
  // this should complete successfully and safely without triggering any checks
  // or races.
  for (auto& r : readers) {
    r->Start();
  }

  // Wait for all reader threads to finish.
  for (auto& r : readers) {
    r->Join();
  }
}

}  // namespace
}  // namespace base::i18n
