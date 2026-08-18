# Chromium & V8 Macro-Optimization Patterns

Proven architectural and engine-level optimization patterns for Speedometer 3
and JetStream.

______________________________________________________________________

## 1. Blink DOM & Layout Engine

### A. FlatTree Traversal & Element Sequence Iteration

- **Problem**: Iterating over DOM collections (`childElementCount`,
  `firstElementChild`, `nextElementSibling`, slot assignments) causes repeated
  tree traversal overhead and pointer chases.
- **Pattern**: Flat DOM strip / inline vector batching. Buffer flat child
  vectors or contiguous arrays to replace $O(N)$ repeated traversal with $O(1)$
  direct array index strides.
- **Key Targets**: `ElementTraversal`, `LayoutTreeBuilderTraversal`,
  `FlatTreeTraversal`.

### B. Canvas 2D Path Batching & Immediate Drawing

- **Problem**: Frameworks (e.g. Chart.js) make thousands of consecutive
  `moveTo`/`lineTo`/`arc` calls per frame, allocating Skia `SkPathBuilder` verbs
  on every API invocation.
- **Pattern**: Contiguous coordinate slab buffering (`Vector<gfx::PointF, 32>`).
  Defer Skia path building until `stroke()` or `fill()`. For single-line strokes
  (`IsLine()`), dispatch direct `c->drawLine` calls to skip path construction
  entirely.
- **Key Targets**: `CanvasPath::LineBuilder`, `CanvasRenderingContext2DState`.

______________________________________________________________________

## 2. V8 JavaScript Engine

### A. IC Stub Cache Expansion & Thrashing Reduction

- **Problem**: Frameworks with polymorphic and megamorphic call sites exceed
  standard primary/secondary stub cache table capacities, causing constant
  eviction and runtime fallback overhead.
- **Pattern**: Expand `kPrimaryTableBits` and `kSecondaryTableBits` in
  `v8/src/ic/stub-cache.h` to retain handlers for diverse receiver maps across
  benchmark iterations.
- **Key Targets**: `StubCache`, `LoadIC`, `StoreIC`,
  `CodeStubAssembler::TryProbeStubCache`.

### B. Maglev / Turboshaft High-Order Builtin Inline Reduction

- **Problem**: High-order array builtins (`Array.prototype.forEach`,
  `Array.prototype.filter`, `Array.prototype.map`) cross boundary accessors,
  falling back to generic bytecode calls.
- **Pattern**: Unroll dense array loops in Maglev IR / Turboshaft with
  speculative checks, converting closure invocations to direct JIT-compiled loop
  strides.
- **Key Targets**: `maglev-graph-builder.cc`, `turboshaft/operations.h`.

### C. GC Allocations & Pointer Compression

- **Problem**: Frequent minor GC pauses during micro-allocations in benchmark
  runner loops.
- **Pattern**: Inline caching, slab allocation for short-lived data structures,
  and eliminating redundant memory clears in table resizes.
