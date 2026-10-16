---
name: general/name-delegate-methods-and-string-representation-getters
rubric: readability
severity: nit
---

**Rule.** Name virtual methods on delegate and provider interfaces descriptively
after the domain artifact they supply rather than using generic object
inspection names like ToString(). When a getter or provider method returns a
formatted string representation of an entity that exists as a dedicated class or
struct (such as StackTrace), include String in the method name (e.g.,
Get<Entity>String() or <entity>\_string()) rather than naming it after the bare
entity (Get<Entity>()).

**Why.** Generic method names like ToString() on delegate or provider interfaces
misleadingly suggest serializing the provider instance itself rather than
retrieving the underlying domain artifact. Qualifying getters that return
serialized strings with String distinguishes string-returning accessors from
methods that return an instance of the rich domain class.

## Examples

Bad:

```cpp
class BASE_EXPORT StackTraceAcrossMojo {
 public:
  class BASE_EXPORT Provider {
   public:
    virtual ~Provider() = default;

    // Ambiguous: implies converting the Provider instance itself to a string.
    virtual std::string ToString() = 0;

    // Ambiguous: implies returning a StackTrace object rather than a formatted string.
    virtual std::string GetStackTrace() = 0;
  };
};
```

Good:

```cpp
class BASE_EXPORT StackTraceAcrossMojo {
 public:
  class BASE_EXPORT Provider {
   public:
    virtual ~Provider() = default;

    // Explicit: names the supplied domain artifact and specifies the string return type.
    virtual std::string GetStackTraceString() = 0;
  };
};
```
