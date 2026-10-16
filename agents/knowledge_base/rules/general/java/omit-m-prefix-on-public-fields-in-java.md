---
name: general/omit-m-prefix-on-public-fields-in-java
rubric: readability
severity: nit
---

**Rule.** In Java, omit the 'm' prefix on public fields (e.g., use 'fileName')
and name them in lowerCamelCase; reserve the 'm' prefix strictly for non-public,
non-static member fields (e.g., 'mFileName') and 's' for static fields, per
Android and Chromium Java style conventions.

**Why.** Chromium follows the Android Open Source Project (AOSP) Java style
conventions. Prefixing public fields with 'm' leaks internal member naming
conventions into external APIs, whereas reserving 'm' for private/protected
fields and 's' for static fields maintains clean, consistent naming across all
Chromium Java code.

## Examples

Bad:

```cpp
public class Location {
    public final String mFileName;
    public final String mFunctionName;
    public final int mLineNumber;

    public Location(String fileName, String functionName, int lineNumber) {
        this.mFileName = fileName;
        this.mFunctionName = functionName;
        this.mLineNumber = lineNumber;
    }
}
```

Good:

```cpp
public class Location {
    public final String fileName;
    public final String functionName;
    public final int lineNumber;

    public Location(String fileName, String functionName, int lineNumber) {
        this.fileName = fileName;
        this.functionName = functionName;
        this.lineNumber = lineNumber;
    }
}
```
