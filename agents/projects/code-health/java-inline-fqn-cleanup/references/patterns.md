# Implementation Patterns: Java Inline FQN Cleanup

This document contains standard patterns and examples for identifying and
cleaning up inline fully qualified Java names (FQNs).

## 📋 Examples & Patterns

### 1. Variables & Types

- **Before:** `android.view.View view = ...`
- **After:** `View view = ...` (with `import android.view.View;` added at top)

### 2. Static Methods & Constants

- **Before:** `org.chromium.build.BuildConfig.IS_CHROME_BRANDED`
- **After:** `BuildConfig.IS_CHROME_BRANDED` (with
  `import org.chromium.build.BuildConfig;` added at top)

### 3. Annotation Markers

- **Before:**
  `@OptIn(markerClass = org.chromium.net.QuicOptions.Experimental.class)`
- **After:** `@OptIn(markerClass = QuicOptions.Experimental.class)` (with
  `import org.chromium.net.QuicOptions;` added at top)

### 4. Method Parameters

- **Before:** `void foo(org.chromium.base.Callback<T> callback)`
- **After:** `void foo(Callback<T> callback)` (with
  `import org.chromium.base.Callback;` added at top)

### 5. Implicitly Imported Classes (java.lang.\*)

- **Before:** `java.lang.String text = ...`
- **After:** `String text = ...` (**DO NOT** add an import statement since
  `java.lang.*` is implicitly imported by Java. Adding it will cause a
  RedundantImport linter error.)

### 6. Resource Classes (*.R.*)

- **Rule:** **DO NOT** modify any FQN containing `.R.` (e.g., `android.R.attr`
  or `org.chromium.chrome.R.id`).
- **Why:** Local `R` classes are almost always imported (e.g.,
  `import org.chromium.chrome.R;`). Importing a secondary `R` class (like
  `android.R`) creates an unresolvable naming collision that breaks the build.
  These MUST remain fully qualified.
