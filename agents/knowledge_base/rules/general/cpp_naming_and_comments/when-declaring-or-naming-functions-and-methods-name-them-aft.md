---
name: general/when-declaring-or-naming-functions-and-methods-name-them-aft
rubric: readability
severity: optional
---

**Rule.** Do not encode a caller's rationale, intent, or desired downstream
consequences in a function or method name (e.g., avoid DoXToPreventY() or
DoXForY()). Name functions after the concrete action or operation they perform
(e.g., DoX()), leaving caller motivation and context to doc comments or call
sites.

**Why.** Embedding caller motivation or consequences in a function name makes
identifiers unnecessarily verbose, couples the function contract to one specific
call site's perspective, and obscures reuse across other contexts that need the
same underlying operation.

## Examples

Bad:

```cpp
// Bad: Encodes the caller's reason and downstream purpose in the function name.
void SkipToEndToPreventFutureReads(SpanReader<const uint8_t>& reader) {
  const bool ok = reader.Skip(reader.remaining()).has_value();
  DCHECK(ok);
}
```

Good:

```cpp
// Good: Named after the operation performed; rationale is left to caller context.
void SkipToEnd(SpanReader<const uint8_t>& reader) {
  const bool ok = reader.Skip(reader.remaining()).has_value();
  DCHECK(ok);
}
```
