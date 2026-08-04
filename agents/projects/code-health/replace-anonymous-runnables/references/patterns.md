# Implementation Patterns: Replace Anonymous Runnables

Use these examples to guide the refactoring.

## Pattern: Prefer Lambdas over Anonymous Classes

Anonymous classes implementing Single Abstract Method (SAM) interfaces add class
metadata overhead. Converting them to lambdas allows D8/R8 to merge them via
Lambda Grouping.

> **What is Lambda Grouping?** If you have multiple lambdas of the same type
> (e.g. `OnClickListener`), D8/R8 merges them into a single compiled class
> containing a `switch` statement under the hood. Explicit anonymous classes
> (e.g., `new OnClickListener() { ... }`) cannot be merged by the compiler and
> are forced to compile as separate individual classes, inflating binary size.

### Bad Pattern

```java
mHandler.post(new Runnable() {
    @Override
    public void run() {
        showKeyboard();
    }
});
```

### Good Pattern

```java
mHandler.post(() -> showKeyboard());
```

### Exception: Annotated Methods (Do NOT Refactor)

If the method in the anonymous class has annotations (e.g.
`@JavascriptInterface`, test annotations, or `@SuppressLint`), it must remain an
anonymous class. Java lambda syntax does not support annotating the implemented
method. Do not attempt to move `@SuppressLint` to the field or enclosing method,
as lint may still fail to suppress the warning on the lambda.

- **Keep as Anonymous Class:**

    ```java
    // Cannot be converted to lambda because run() requires @JavascriptInterface
    webView.addJavascriptInterface(new Runnable() {
      @Override
      @JavascriptInterface
      public void run() {
          doSomething();
      }
    }, "Host");
    ```

### Exception: Use of `this` (Do NOT Refactor)

If the anonymous class body uses `this` to refer to the anonymous class instance
itself, it cannot be converted to a lambda.

- **Keep as Anonymous Class:**

    ```java
    mHandler.post(new Runnable() {
        @Override
        public void run() {
            // 'this' refers to the Runnable instance, e.g., to remove itself
            mHandler.removeCallbacks(this);
        }
    });

    ```

### Exception: References to Blank Final Fields in Field Initializers (Do NOT Refactor)

If the anonymous class is defined as a field initializer, and its body
references a `final` field that is initialized in the constructor (a blank final
field), it cannot be converted to a lambda. Lambdas in field initializers cannot
reference fields that are not yet definitely assigned.

- **Keep as Anonymous Class:**

    ```java
    class MyComponent {
        // mStartSmoothIndeterminate is a field initializer
        private final Runnable mStartSmoothIndeterminate =
                new Runnable() {
                    @Override
                    public void run() {
                        // mAnimationLogic is blank final and initialized in constructor.
                        // Converting this to a lambda will cause a compile error:
                        // "variable mAnimationLogic might not have been initialized"
                        mAnimationLogic.reset();
                    }
                };

      private final AnimationLogic mAnimationLogic;

      public MyComponent() {
          mAnimationLogic = new AnimationLogic();
      }
    }
    ```

### Exception: View.OnTouchListener (Do NOT Refactor)

Do not convert `View.OnTouchListener` to a lambda if it does not call
`View#performClick()`. This will trigger a `ClickableViewAccessibility` lint
warning (either new or by breaking a lint baseline), and suppressing this
warning on a lambda is difficult without broad suppression annotations.

- **Keep as Anonymous Class:**

    ```java
    mView.setOnTouchListener(new View.OnTouchListener() {
      @Override
      public boolean onTouch(View v, MotionEvent event) {
          // Does not call v.performClick()
          mGestureDetector.onTouchEvent(event);
          return false;
      }
    });
    ```
