# Java Asserts in Chromium
This doc exists to explain how asserts in Java are enabled and disabled by
Chromium's build system.

## javac Assertion Bytecode
Whenever javac compiles a Java class, assertions are transformed into the
following bytecode:

```
    Code:
       0: getstatic     #2            // Static field $assertionsDisabled
       3: ifne          20            // Conditional jump past assertion throw
      12: new           #3            // Class java/lang/AssertionError
      19: athrow                      // Throwing AssertionError
      20: return

// NOTE: this static block was made just to check the desiredAssertionStatus.
// There was no static block on the class before javac created one.
  static {};
    Code:
       2: invokevirtual #6            // Method java/lang/Class.desiredAssertionStatus()
       5: ifne          12
       8: iconst_1
       9: goto          13
      12: iconst_0
      13: putstatic     #2            // Static field $assertionsDisabled
      16: return
```

TL;DR - every single assertion is gated behind a `assertionDisabled` flag check,
which is a static field that can be set by the JRE's
`setDefaultAssertionStatus`, `setPackageAssertionStatus`, and
`setClassAssertionStatus` methods.

## Assertion Enabling/Disabling
Our tools which consume javac output, namely R8 and D8, each have flags which
the build system uses to enable or disable asserts. We control this with the
`enable_java_asserts` gn arg. It does this by deleting the gating check on
`assertionsDisabled` when enabling, and by eliminating any reference to the
assert when disabling.

```java
// Example equivalents of:
a = foo();
assert a != 0;
return a;

// Traditional, unoptimized javac output.
a = foo();
if (!assertionsDisabled && a == 0) {
  throw new AssertionError();
}
return a;

// Optimized with assertions enabled.
a = foo();
if (a == 0) {
  throw new AssertionError();
}
return a;

// Optimized with assertions disabled.
a = foo();
return a;
```

## Assertion Enabling on Canary
Recently we [enabled
asserts](https://chromium-review.googlesource.com/c/chromium/src/+/3307087) on
Canary. It spiked our crash rate, and it was decided to not do this again, as
it's bad user experience to crash the app incessantly for non-fatal issues.

So, we asked the R8 team for a feature which would rewrite the bytecode of these
assertions, which they implemented for us. Now, instead of just turning it on
and throwing an `AssertionError`, [R8 would call a provided assertion
handler](https://r8.googlesource.com/r8/+/aefe7bc18a7ce19f3e9c6dac0bedf6d182bbe142/src/main/java/com/android/tools/r8/ParseFlagInfoImpl.java#124)
with the `AssertionError`. We then wrote a [silent assertion
reporter](https://chromium-review.googlesource.com/c/chromium/src/+/3746261)
and this reports Java `AssertionErrors` to our crash server without crashing
the browser.
