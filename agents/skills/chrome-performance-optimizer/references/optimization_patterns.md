# Chromium & V8 Macro-Optimization Patterns

Proven architectural and engine-level optimization patterns for Speedometer 3
and JetStream, derived from top historical benchmark wins. Use this reference
when analyzing CPU and allocation profiles to identify high-leverage engine
optimizations.

______________________________________________________________________

## 1. Blink Style & Cascade Engine

### A. StyleSheet & RuleSet Deduplication in `MatchRequest`

- **Benchmark Win**: **+1.50%** Speedometer 3 total score (CL
  [5385787](https://chromium-review.googlesource.com/c/chromium/src/+/5385787)).
- **Subtest Impact**: TodoMVC-Svelte (`-11.1%`), TodoMVC-Preact (`-12.6%`),
  TodoMVC-React (`-7.3%`).
- **Profile Signature**: Heavy time spent in
  `ElementRuleCollector::CollectMatchingRules`, `MatchRequest`, and `RuleSet`
  matching across identical inline `<style>` blocks.
- **Problem**: Modern component frameworks (Svelte, Preact, WebComponents) emit
  hundreds of identical `<style>` blocks inside shadow roots or documents. While
  `StyleSheetContents` was deduplicated, `MatchRequest` collected separate
  `RuleSet` entries for each identical instance, resulting in repeated selector
  matching and cascade evaluation.
- **Pattern**: Deduplicate `RuleSet` pointers during `MatchRequest` construction
  when backed by the same `StyleSheetContents`. Skip redundant selector matching
  for all subsequent duplicate sheets.
- **Key Targets**:
  `third_party/blink/renderer/core/css/resolver/match_request.h`,
  `element_rule_collector.cc`.

### B. Matched Properties Cache (MPC) Optimization

- **Benchmark Win**: **+1.00%** (CL
  [5173590](https://chromium-review.googlesource.com/c/chromium/src/+/5173590)),
  **+0.71%** (CL
  [5942287](https://chromium-review.googlesource.com/c/chromium/src/+/5942287)).
- **Subtest Impact**: TodoMVC-Lit (`-14.0%`), TodoMVC-React (`-7.3%`),
  TodoMVC-Backbone (`-3.1%`).
- **Profile Signature**: High cache miss rates in `MatchedPropertiesCache`,
  heavy overhead in `StyleResolver::ApplyMatchedProperties`, or expensive
  partial cache hit re-evaluations.
- **Patterns**:
  1. **By-Value Inheritance Comparison**: When testing candidate `ComputedStyle`
     entries in the MPC, compare inherited property data by value rather than
     pointer identity, enabling cache hits across structurally identical
     ancestor styles.
  2. **Multi-Candidate Hash Buckets**: Allow holding multiple `ComputedStyle`
     candidates per hash key in the MPC to accommodate differing inherited
     parent contexts.
  3. **Eliminating Partial MPC Hits**: Partial application (applying only
     non-inherited properties and retrofitting inherited ones) frequently
     triggers secondary re-computations that cost more than clean misses;
     replacing partial hits with multi-candidate full matches yields substantial
     wins.
- **Key Targets**:
  `third_party/blink/renderer/core/css/resolver/matched_properties_cache.cc`,
  `style_resolver.cc`.

### C. Selector Bucketing & Fast Rejection Maps

- **Benchmark Win**: **+0.90%** (CL
  [6632977](https://chromium-review.googlesource.com/c/chromium/src/+/6632977)).
- **Profile Signature**: `RuleSet::FindBestRuleSetAndAdd` iterating broad
  tag/attribute buckets for frequent inputs (e.g. `input[type="checkbox"]`,
  `input[type="text"]`).
- **Problem**: High-frequency element selectors with specialized attributes
  (like form controls) ended up in generic tag buckets, forcing every `input`
  element to test against dozens of irrelevant rules.
- **Pattern**: Introduce dedicated bucket maps for common selector patterns
  (e.g. `input[type="..."]`, `:focus`, `:focus-visible`).
- **Key Targets**: `third_party/blink/renderer/core/css/rule_set.cc`,
  `rule_set.h`.

### D. Eliminating Dead UA Stylesheet Properties & Features

- **Benchmark Win**: **+0.40%** to **+0.50%** (CL
  [7277167](https://chromium-review.googlesource.com/c/chromium/src/+/7277167)).
- **Profile Signature**: Global style matching checking properties like
  `overlay: auto` across every element in top-layer resolution.
- **Pattern**: Strip obsolete CSS properties and replace global UA rule
  evaluation with direct element flags (e.g. top layer bit checks).
- **Key Targets**:
  `third_party/blink/renderer/core/css/resolver/style_adjuster.cc`.

______________________________________________________________________

## 2. Blink Layout, Text & Font Shaping

### A. HarfBuzz Apple Advanced Typography (AAT) Fast Path

- **Benchmark Win**: **+1.20%** (CL
  [5538168](https://chromium-review.googlesource.com/c/chromium/src/+/5538168)),
  **+0.70%** (CL
  [6239201](https://chromium-review.googlesource.com/c/chromium/src/+/6239201)).
- **Subtest Impact**: Editor-TipTap (`-7%` to `-10%`), Editor-CodeMirror,
  NewsSite-Next.
- **Profile Signature**: Time spent in `hb_aat_layout_shape`,
  `hb_aat_layout_morx_table`, ligature state machines, and kerning pair searches
  for core macOS system fonts (Helvetica, Times).
- **Patterns**:
  1. **Kerning Pair Search Caching**: Maintain a compact (128-entry / 256-byte)
     lookup cache for kerning pair searches, replacing `hb_set_digest` with full
     `hb_set_t` coverage checks.
  2. **Actionable Subchain Filtering**: Compute the set of glyphs that trigger
     initial state transitions for each AAT subchain, bypassing whole state
     machines for buffers without triggering glyphs.
- **Key Targets**: `third_party/harfbuzz-ng/`,
  `third_party/blink/renderer/platform/fonts/shaping/`.

### B. Global & Short-Text Font Shape Caching

- **Benchmark Win**: **+0.80%** (CL
  [5961246](https://chromium-review.googlesource.com/c/chromium/src/+/5961246),
  CL
  [6188529](https://chromium-review.googlesource.com/c/chromium/src/+/6188529)).
- **Profile Signature**: Repeated calls to `HarfBuzzShaper::Shape` on short
  strings (e.g. label text, single numbers, short tokens).
- **Pattern**: Implement a global/thread-local `ShapeCache` (`NSShapeCache`)
  keyed on text run hash, font description, and direction, avoiding HarfBuzz
  invocation entirely for repeated short strings.
- **Key Targets**:
  `third_party/blink/renderer/platform/fonts/shaping/shape_cache.h`,
  `caching_word_shaper.cc`.

### C. Text Line Break Iteration (`LazyLineBreakIterator`)

- **Benchmark Win**: **+1.20%** (CL
  [5401325](https://chromium-review.googlesource.com/c/chromium/src/+/5401325),
  CL
  [5382280](https://chromium-review.googlesource.com/c/chromium/src/+/5382280)).
- **Subtest Impact**: Editor-TipTap (`-11%`), Charts-observable-plot (`-3.5%`),
  TodoMVC-Preact (`-2.5%`).
- **Profile Signature**: ICU `BreakIterator::following` scanning from offset to
  start of string, or frequent ICU boundary queries in `LineBreaker`.
- **Patterns**:
  1. **Resetting ICU Iterator for Min-Max**: In `HandleTextForFastMinContent`,
     reset the ICU `BreakIterator` per break opportunity rather than retaining a
     single scanning iterator over entire text chunks.
  2. **Extended Fast Table**: Extend the Latin-1 fast break lookup table from
     `U+007F` up to `U+00FF` (C1 controls & Latin-1 supplement), avoiding ICU
     calls for Western European text.
- **Key Targets**:
  `third_party/blink/renderer/platform/text/text_break_iterator.cc`,
  `line_breaker.cc`.

### D. Devirtualizing Core DOM & Layout Type Checks

- **Benchmark Win**: **+1.10%** (CL
  [5067069](https://chromium-review.googlesource.com/c/chromium/src/+/5067069)).
- **Profile Signature**: Indirect branch mispredictions and vtable lookup
  overhead in `LayoutObject::IsLayoutBlockFlow()`, `LayoutObject::IsBox()`,
  `LayoutObject::IsText()`.
- **Pattern**: Devirtualize type-checking methods by storing bitfields directly
  on base classes (`LayoutObject`, `Node`), turning indirect virtual calls into
  single bitwise instructions.
- **Key Targets**: `third_party/blink/renderer/core/layout/layout_object.h`.

______________________________________________________________________

## 3. Blink DOM Parsing, Construction & Mutation

### A. HTML Fast-Path Parser Routing

- **Benchmark Win**: **+1.00%** (CL
  [5186796](https://chromium-review.googlesource.com/c/chromium/src/+/5186796)).
- **Subtest Impact**: NewsSite-Nuxt (`-15%`), NewsSite-Next.
- **Profile Signature**: `DOMParser.parseFromString` spending CPU cycles inside
  full `HTMLDocumentParser` tokenizers and tree builders.
- **Pattern**: Route `DOMParser.parseFromString` through `HTMLFastPathParser`
  for small/medium HTML payloads with `<body>` parent tags and quirks mode
  support.
- **Key Targets**:
  `third_party/blink/renderer/core/html/parser/html_document_parser_fastpath.cc`,
  `dom_parser.cc`.

### B. Lazy Initialization of Document & Form State

- **Benchmark Win**: **+1.00%** (CL
  [5260173](https://chromium-review.googlesource.com/c/chromium/src/+/5260173),
  CL
  [5268621](https://chromium-review.googlesource.com/c/chromium/src/+/5268621),
  CL
  [5823214](https://chromium-review.googlesource.com/c/chromium/src/+/5823214)).
- **Profile Signature**: Hot object allocation in constructors
  (`Document::Document()`, `HTMLInputElement::Create()`) allocating members
  never used in benchmark lifecycles.
- **Patterns**:
  1. **Delaying Form Editors**: Defer instantiation of `Editor` in form elements
     until text input or focus occurs.
  2. **Lazy Document Tokens & Services**: Instantiate `DocumentToken`,
     `VisitedLinkState`, and `RenderBlockingResourceManager` lazily upon first
     access rather than unconditionally in constructor bodies.
- **Key Targets**: `third_party/blink/renderer/core/dom/document.cc`,
  `html_input_element.cc`.

### C. Reworking Observer Sets for Iteration Performance

- **Benchmark Win**: **+0.40%** to **+0.60%** (CL
  [5828781](https://chromium-review.googlesource.com/c/chromium/src/+/5828781)).
- **Profile Signature**: `HeapHashSet` iteration overhead during frequent
  synchronous DOM mutation observer dispatching.
- **Pattern**: Replace `HeapHashSet` with a vector-backed `HeapObserverSet` to
  optimize for peak sequential iteration speeds on every DOM modification.
- **Key Targets**: `third_party/blink/renderer/core/dom/heap_observer_set.h`.

### D. SVG Path Parsing Cache

- **Benchmark Win**: **+0.40%** (CL
  [7496492](https://chromium-review.googlesource.com/c/chromium/src/+/7496492)).
- **Profile Signature**: `SVGPathParser::ParsePath` re-parsing identical path
  strings in SVG charts (e.g. React-Stockcharts-SVG).
- **Pattern**: Add a bounded LRU cache mapping `AtomicString` path definitions
  to parsed `SVGPathByteStream` data.
- **Key Targets**: `third_party/blink/renderer/core/svg/svg_path_parser.cc`.

______________________________________________________________________

## 4. V8 & Memory Management (GC / Oilpan)

### A. Oilpan (cppgc) Sweeping in Idle Tasks

- **Benchmark Win**: **+2.00%** (CL
  [5454401](https://chromium-review.googlesource.com/c/v8/v8/+/5454401), CL
  [5490661](https://chromium-review.googlesource.com/c/chromium/src/+/5490661)).
- **Profile Signature**: Main thread mutator latency spikes due to synchronous
  sweeping on allocation in `NormalPageSpace::Allocate`.
- **Pattern**: Move Oilpan sweeping and finalizer execution to concurrent worker
  threads and scheduled idle tasks. Avoid synchronous sweeping during allocation
  unless nearing heap exhaustion. Sort sweeping order by live byte counts to
  recycle large free blocks first.
- **Key Targets**: `v8/src/heap/cppgc/sweeper.cc`,
  `v8/src/heap/cppgc-js/cpp-heap.cc`.

### B. Minor GC Scheduling & Task Prioritization

- **Benchmark Win**: **+1.00%** (CL
  [5447695](https://chromium-review.googlesource.com/c/v8/v8/+/5447695), CL
  [5934857](https://chromium-review.googlesource.com/c/v8/v8/+/5934857)),
  **+0.50%** (CL
  [6070380](https://chromium-review.googlesource.com/c/v8/v8/+/6070380)).
- **Subtest Impact**: TodoMVC-jQuery (`-6.5%`), TodoMVC-JavaScript-ES6
  (`-2.5%`).
- **Patterns**:
  1. **Page Navigation Minor GC**: Trigger idle minor GC on context disposal to
     clear garbage before next page/benchmark iteration starts.
  2. **Embedder Marking Speed Normalization**: Normalize Blink Oilpan marking
     speeds in unified heap GC pacing to prevent over-aggressive concurrent
     marking starts.
  3. **Lower Priority Minor GC Tasks**: Downgrade minor GC task priorities so
     high-priority user-visible rendering tasks are not blocked by speculative
     minor collections.
- **Key Targets**: `v8/src/heap/heap.cc`, `v8/src/heap/gc-tracer.cc`.

### C. Sparkplug+ Embedded Feedback (EFB)

- **Benchmark Win**: **+0.70%** (CL
  [7878264](https://chromium-review.googlesource.com/c/v8/v8/+/7878264)).
- **Profile Signature**: FeedbackVector lookup overhead in baseline JIT code for
  arithmetic and unary operations.
- **Pattern**: Embed type feedback indices directly in BytecodeArray operands
  (`kEmbeddedFeedback`), avoiding FeedbackVector slot accesses in Sparkplug and
  TSA builtins.
- **Key Targets**: `v8/src/interpreter/bytecode-generator.cc`,
  `v8/src/baseline/baseline-compiler.cc`.

### D. Shared Mutex Implementation on macOS

- **Benchmark Win**: **+0.60%** (CL
  [6088023](https://chromium-review.googlesource.com/c/v8/v8/+/6088023)).
- **Profile Signature**: `std::shared_mutex` spending substantial CPU time
  inside Darwin kernel syscalls (`__psynch_rw_wrlock`).
- **Pattern**: Replace `std::shared_mutex` with `absl::Mutex` in V8 SharedMutex
  on macOS.
- **Key Targets**: `v8/src/base/platform/shared-mutex.h`.

### E. MicrotaskQueue Copy-on-Write (COW)

- **Benchmark Win**: **+0.40%** (CL
  [5670360](https://chromium-review.googlesource.com/c/v8/v8/+/5670360)).
- **Profile Signature**: Memory allocations and vector copying in
  `MicrotaskQueue::RunMicrotasks` for completion callbacks.
- **Pattern**: Use copy-on-write (COW) semantics for completion callback vectors
  so execution iterates without allocating copies.
- **Key Targets**: `v8/src/execution/microtask-queue.cc`.

______________________________________________________________________

## 5. 2D Canvas, Graphics & Memory Primitives

### A. Dedicated Canvas PaintOps for Primitives (Arcs / Ovals / Lines)

- **Benchmark Win**: **+0.53%** (CL
  [5368259](https://chromium-review.googlesource.com/c/chromium/src/+/5368259),
  CL
  [5381631](https://chromium-review.googlesource.com/c/chromium/src/+/5381631)).
- **Subtest Impact**: Charts-chartjs (`-10%`), Charts-observable-plot.
- **Profile Signature**: `SkPath::arcTo` triggering repeated `SkPathRef`
  reallocation during canvas draw loops.
- **Pattern**: Add dedicated `PaintOp` types for primitive shapes (lines, arcs,
  ovals) in `CanvasPath` and `BaseRenderingContext2D`. Call `incReserve()` to do
  a single allocation, or bypass `SkPath` construction entirely for straight
  segments.
- **Key Targets**:
  `third_party/blink/renderer/modules/canvas/canvas2d/canvas_path.cc`,
  `canvas_rendering_context_2d.cc`.

### B. High-Performance String Hashing (`rapidhash`)

- **Benchmark Win**: **+0.50%** (CL
  [5667077](https://chromium-review.googlesource.com/c/chromium/src/+/5667077)).
- **Subtest Impact**: TodoMVC-jQuery (`-2.2%`), TodoMVC-Lit (`-1.7%`),
  NewsSite-Nuxt (`-1.6%`).
- **Profile Signature**: Time spent in `StringHasher::HashCharacters` /
  `SuperFastHash` across `AtomicString` table lookups.
- **Pattern**: Replace outdated hash functions (SuperFastHash) with `rapidhash`
  for 32/64-bit chunk throughput with zero quality collisions.
- **Key Targets**:
  `third_party/blink/renderer/platform/wtf/text/string_hasher.h`.

### C. Inlined Storage Zeroing Elimination

- **Benchmark Win**: **+0.50%** (CL
  [6563749](https://chromium-review.googlesource.com/c/chromium/src/+/6563749)).
- **Profile Signature**: Redundant `memset` instructions in constructor bodies
  of `HeapVector` with inline capacity.
- **Pattern**: Avoid clearing inline storage upfront when elements will be
  immediately constructed in place.
- **Key Targets**:
  `third_party/blink/renderer/platform/heap/collection_support/heap_vector.h`.

______________________________________________________________________

## 6. Profile Guided Optimization (PGO) & Toolchain

### A. Value Profiling Tuning (`-vp-counters-per-site`)

- **Benchmark Win**: **+0.80%** (CL
  [7501095](https://chromium-review.googlesource.com/c/chromium/src/+/7501095)).
- **Profile Signature**: Indirect call branch targets mispredicted on Apple
  Silicon hardware (AArch64).
- **Pattern**: Tune LLVM Clang PGO `-vp-counters-per-site` to collect richer
  value profiles for indirect function call devirtualization.
- **Key Targets**: `build/config/compiler/pgo/BUILD.gn`.

### B. Benchmark Weighting in PGO Bot Profiles

- **Benchmark Win**: **+0.70%** (CL
  [5825299](https://chromium-review.googlesource.com/c/chromium/src/+/5825299)).
- **Pattern**: Increase the iteration weighting of modern workloads (Speedometer
  3\) during PGO profile generation runs on builders.
- **Key Targets**: `tools/perf/process_perf_results.py`, Chromium build scripts.

______________________________________________________________________

## 7. Historical Top 20 Benchmark Wins Summary

| Rank |   Impact   | Optimization                            | CL Link                                                                                  |                        Tracking Bug                         |
| :--: | :--------: | :-------------------------------------- | :--------------------------------------------------------------------------------------- | :---------------------------------------------------------: |
|  1   | **+2.00%** | Oilpan sweeping in idle time            | [crrev.com/c/5454401](https://chromium-review.googlesource.com/c/v8/v8/+/5454401)        | [b/333981063](https://issues.chromium.org/issues/333981063) |
|  2   | **+1.50%** | Deduplicate RuleSets in MatchRequest    | [crrev.com/c/5385787](https://chromium-review.googlesource.com/c/chromium/src/+/5385787) |                              —                              |
|  3   | **+1.20%** | HarfBuzz AAT shaping roll               | [crrev.com/c/5538168](https://chromium-review.googlesource.com/c/chromium/src/+/5538168) |  [b/40943591](https://issues.chromium.org/issues/40943591)  |
|  4   | **+1.20%** | `LazyLineBreakIterator` min-max reset   | [crrev.com/c/5401325](https://chromium-review.googlesource.com/c/chromium/src/+/5401325) |  [b/41485013](https://issues.chromium.org/issues/41485013)  |
|  5   | **+1.10%** | Devirtualize `LayoutObject` type checks | [crrev.com/c/5067069](https://chromium-review.googlesource.com/c/chromium/src/+/5067069) |  [b/40943149](https://issues.chromium.org/issues/40943149)  |
|  6   | **+1.00%** | `DOMParser` HTML fast-path parser       | [crrev.com/c/5186796](https://chromium-review.googlesource.com/c/chromium/src/+/5186796) |   [b/1517086](https://issues.chromium.org/issues/1517086)   |
|  7   | **+1.00%** | Delay creation of form editors          | [crrev.com/c/5260173](https://chromium-review.googlesource.com/c/chromium/src/+/5260173) | [b/325613706](https://issues.chromium.org/issues/325613706) |
|  8   | **+1.00%** | Lazy creation of state in Document      | [crrev.com/c/5268621](https://chromium-review.googlesource.com/c/chromium/src/+/5268621) |  [b/41497266](https://issues.chromium.org/issues/41497266)  |
|  9   | **+1.00%** | MPC inherited data by-value comparison  | [crrev.com/c/5173590](https://chromium-review.googlesource.com/c/chromium/src/+/5173590) |  [b/40945016](https://issues.chromium.org/issues/40945016)  |
|  10  | **+1.00%** | Minor GC on context disposal            | [crrev.com/c/5447695](https://chromium-review.googlesource.com/c/v8/v8/+/5447695)        | [b/333423696](https://issues.chromium.org/issues/333423696) |
|  11  | **+1.00%** | Normalize embedder GC marking speed     | [crrev.com/c/5934857](https://chromium-review.googlesource.com/c/v8/v8/+/5934857)        | [b/373759996](https://issues.chromium.org/issues/373759996) |
|  12  | **+0.90%** | Bucket map for `input[type="…"]`        | [crrev.com/c/6632977](https://chromium-review.googlesource.com/c/chromium/src/+/6632977) | [b/402346409](https://issues.chromium.org/issues/402346409) |
|  13  | **+0.80%** | `NSShapeCache` for font shaping         | [crrev.com/c/5961246](https://chromium-review.googlesource.com/c/chromium/src/+/5961246) | [b/377832797](https://issues.chromium.org/issues/377832797) |
|  14  | **+0.80%** | PGO `-vp-counters-per-site` on Mac      | [crrev.com/c/7501095](https://chromium-review.googlesource.com/c/chromium/src/+/7501095) | [b/479721693](https://issues.chromium.org/issues/479721693) |
|  15  | **+0.71%** | Multi-candidate MPC entries             | [crrev.com/c/5942287](https://chromium-review.googlesource.com/c/chromium/src/+/5942287) |  [b/40068819](https://issues.chromium.org/issues/40068819)  |
|  16  | **+0.70%** | Speedometer weight in PGO bots          | [crrev.com/c/5825299](https://chromium-review.googlesource.com/c/chromium/src/+/5825299) | [b/363195532](https://issues.chromium.org/issues/363195532) |
|  17  | **+0.70%** | HarfBuzz AAT workshop roll              | [crrev.com/c/6239201](https://chromium-review.googlesource.com/c/chromium/src/+/6239201) | [b/392652317](https://issues.chromium.org/issues/392652317) |
|  18  | **+0.70%** | Sparkplug embedded feedback (EFB)       | [crrev.com/c/7878264](https://chromium-review.googlesource.com/c/v8/v8/+/7878264)        | [b/429351411](https://issues.chromium.org/issues/429351411) |
|  19  | **+0.60%** | Mac shared mutex `absl::Mutex`          | [crrev.com/c/6088023](https://chromium-review.googlesource.com/c/v8/v8/+/6088023)        | [b/383975879](https://issues.chromium.org/issues/383975879) |
|  20  | **+0.53%** | SkPath canvas arc/oval allocation avoid | [crrev.com/c/5368259](https://chromium-review.googlesource.com/c/chromium/src/+/5368259) | [b/329896153](https://issues.chromium.org/issues/329896153) |

______________________________________________________________________

## 8. Diagnostic Checklist for Optimization Hunting

1. **Check Profile Call Stacks**:
   - Is time spent in ICU or HarfBuzz shaping? -> Look for iterator resets, word
     caches, or AAT fast-path opportunities.
   - Is time spent in `ElementRuleCollector` or style recalc? -> Check for
     stylesheet deduplication, MPC by-value hits, or selector bucketing.
   - Is time spent in Oilpan / GC allocation paths? -> Check for lazy object
     member initialization, idle sweeping deferrals, or vector zeroing
     elimination.
   - Is time spent in JIT stubs / bytecodes? -> Look for embedded feedback (EFB)
     and IC cache capacity limits.
2. **Verify Benchmark Independence**:
   - Verify changes against both individual subtest gains and overall geometric
     mean score.
   - Run 150-iteration Pinpoint jobs on M1 hardware to filter noise ($p \<
     0.05$).
