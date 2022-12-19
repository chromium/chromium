# native_inference code

Given a trained TensorFlow model, tf.native generates C++ code to run the model
on an input example and return a value. This code is generated offline and
committed at `native_inference.h` and `native_inference.cc`.

## Prettify native_inference code

A lot of the generated code can be removed or simplified:

* Unused functions and variables
* `#ifdef` wrappers that are never triggered, and macros that expand to no-ops
* Generated variable and namespace names
* `#define` constants instead of scoped `constexpr`s

Approximately the following steps will clean up the generated code:

 1. Delete code inside the `USE_EIGEN` and `OP_LIB_BENCHMARK` `#ifdef`s, and
    delete those `#ifdef` wrappers (a utility like `unifdefall` may help). Also
    delete usage of no-op macros such as `BENCHMARK_TIMER`.
 1. Delete unused functions and macros from native_inference.cc.
 1. Update the namespacing so all code is within the
    `tab_ranker::native_inference` namespace.
 1. Update the `native_inference.h` location included in native_inference.cc
 1. Remove unused variables defined in native_inference.cc, e.g.
    `dnn_input_from_feature_columns_input_from_feature_columns_concat0Shape`.
 1. Rename parameters to the `Inference` function.
 1. Replace constants in native_inference.h with `constexpr int`s for weight,
    feature and bias sizes, and improve their names.
 1. Replace literal constants in native_inference.cc with the named constants
    from native_inference.h.
 1. Replace `assert()` calls with `CHECK()`, and include `"base/logging.h"`.
 1. Remove unused includes, including `<cassert>`.
 1. Add Chromium header comments.

## Updating the model

When updating the model, it may be easier to keep the existing
native_inference.* files and simply replace the constants, including weights,
biases, and array sizes. Check if the generated native_inference.cc file
uses functions that may need to be added back to the prettified version or calls
these functions in a different order.
