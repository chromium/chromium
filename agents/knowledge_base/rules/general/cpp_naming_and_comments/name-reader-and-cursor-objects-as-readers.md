---
name: general/name-reader-and-cursor-objects-as-readers
rubric: readability
severity: should_fix
---

**Rule.** Name stateful reader, writer, or cursor objects (such as
base::SpanReader, base::SpanWriter, or BufferIterator) with a descriptive
reader, writer, or cursor identifier or suffix (e.g., reader\_,
payload_reader\_, or cursor\_) rather than directly naming them after the
underlying static container or payload (e.g., payload\_, buffer\_, or data\_).

**Why.** Stateful readers and cursors maintain dynamic consumption state,
including an advancing offset and remaining unread bytes. Naming them after the
underlying container misleadingly implies they represent the static, complete
payload rather than an active stream or slice cursor over it.

## Examples

Bad:

```cpp
class PickleIterator {
 private:
  base::SpanReader<const uint8_t> payload_;
};
```

Good:

```cpp
class PickleIterator {
 private:
  base::SpanReader<const uint8_t> reader_;
};
```
