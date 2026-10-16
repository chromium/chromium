---
name: general/avoid-duplicate-inline-urls-in-markdown-with-reference-links
rubric: readability
severity: nit
---

**Rule.** In Markdown documents that maintain reference-style link definitions
(`[label]: url`), keep reference definitions synchronized with document
citations: use reference syntax (`[label]` or `[text][label]`) instead of
repeating URLs inline (`[label](url)`), and remove orphaned reference
definitions whose labels are no longer cited.

**Why.** Defining a URL both inline and in a reference block creates redundancy
and risks the targets falling out of sync. Similarly, keeping unreferenced or
orphaned link definitions clutters documentation sources with dead references
and confuses maintainers about active links.

## Examples

Bad:

```cpp
**Why fuzz?** Fuzzing finds thousands of security and stability issues before
they reach users (see [go/fuzzing-success](http://go/fuzzing-success)). For more
information about fuzzing benefits, see [go/why-fuzz].

[go/fuzzing-success]: http://go/fuzzing-success
[go/why-fuzz]: http://go/why-fuzz
[Guided in-process fuzzing]: https://security.googleblog.com/2016/08/guided-in-process-fuzzing-of-chrome.html
```

Good:

```cpp
**Why fuzz?** Fuzzing finds thousands of security and stability issues before
they reach users (see [go/fuzzing-success]). For more information about fuzzing
benefits, see [go/why-fuzz].

[go/fuzzing-success]: http://go/fuzzing-success
[go/why-fuzz]: http://go/why-fuzz
```
