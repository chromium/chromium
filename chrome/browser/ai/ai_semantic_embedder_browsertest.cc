// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features_generated.h"

class AISemanticEmbedderBrowserTest : public InProcessBrowserTest {
 public:
  AISemanticEmbedderBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kAIEmbeddingsAPI}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AISemanticEmbedderBrowserTest, CreateAndEmbed) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL("chrome://newtab/")));
  const char kScript[] = R"(
    (async () => {
      try {
        const availability = await SemanticEmbedder.availability();
        if (availability !== 'available') return 'SKIPPED';

        const embedder =
            await SemanticEmbedder.create();
        if (!embedder) return 'Failed to create';

        function cosineSimilarity(a, b) {
          let dot = 0; let normA = 0; let normB = 0;
          for (let i = 0; i < a.length; i++) {
            dot += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
          }
          return dot / (Math.sqrt(normA) * Math.sqrt(normB));
        }

        let resHighA = await embedder.embed(
            "The chef prepared a delicious meal for the guests.");
        let highA = resHighA.embeddings[0].values;
        let resHighB = await embedder.embed(
            "A tasty dinner was cooked by the chef for the visitors.");
        let highB = resHighB.embeddings[0].values;
        let high = cosineSimilarity(highA, highB);

        let resMedA = await embedder.embed(
            "She is an expert in machine learning.");
        let medA = resMedA.embeddings[0].values;
        let resMedB = await embedder.embed(
            "He has a deep interest in artificial intelligence.");
        let medB = resMedB.embeddings[0].values;
        let med = cosineSimilarity(medA, medB);

        let resLowA = await embedder.embed(
            "The weather in Tokyo is sunny today.");
        let lowA = resLowA.embeddings[0].values;
        let resLowB = await embedder.embed(
            "I need to buy groceries for the week.");
        let lowB = resLowB.embeddings[0].values;
        let low = cosineSimilarity(lowA, lowB);

        let batch_result = await embedder.embed(["hello", "world"]);
        if (!batch_result || batch_result.embeddings.length !== 2) {
          return "FAIL: batch result length mismatch";
        }

        if (high > 0.8 && high > med && med > low && low < 0.6) {
          return "PASS";
        } else {
          return `FAIL: high=${high.toFixed(6)}, ` +
                 `med=${med.toFixed(6)}, low=${low.toFixed(6)}`;
        }
      } catch (e) {
        return e.toString();
      }
    })();
  )";
  std::string result = content::EvalJs(web_contents, kScript).ExtractString();
  if (result == "SKIPPED") {
    // TODO(crbug.com/428233906): Add a command-line test override in the
    // Component Updater logic to sideload `dummy_gemma_model.tflite`.
    // Once that is checked in, we can stop skipping and validate the full
    // execution path on the CI bots.
    GTEST_SKIP() << "Semantic embedder model is not available.";
  }
  EXPECT_EQ(result, "PASS");
}
