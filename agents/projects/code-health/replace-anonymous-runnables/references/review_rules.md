# Review Rules: Replace Anonymous Runnables

- **No Annotations on Overridden Methods:** Verify that none of the converted
  anonymous classes had annotations (like `@JavascriptInterface` or
  `@SuppressLint`) on their methods. Lambdas cannot have annotations on the
  implemented method, and moving them (e.g. `@SuppressLint`) to the field or
  enclosing method to bypass this is not allowed.
- **Correct `this` Reference:** Ensure that the original anonymous class did not
  use `this` to refer to its own instance. If it did, the conversion to lambda
  is incorrect because `this` now refers to the enclosing class.
- **No Blank Final Field References in Field Initializers:** Verify that the
  converted anonymous class was not in a field initializer and referencing blank
  final fields initialized in the constructor (which causes compile errors).
- **No `View.OnTouchListener` Conversions:** Although `OnTouchListener` is a SAM
  interface, it must not be converted to a lambda because it often triggers
  `ClickableViewAccessibility` lint warnings and can break existing lint
  baselines.
