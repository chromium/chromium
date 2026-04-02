# raw_ref&lt;T&gt;

`raw_ref<T>` is a non-null smart pointer for C++ references, providing
Use-after-Free (UaF) protection identical to `raw_ptr<T>`. Think of it as a
combination of `std::reference_wrapper` and `raw_ptr`.

See [raw_ptr.md](./raw_ptr.md) for details on the underlying MiraclePtr
protection.

## Usage

Use `raw_ref<T>` for class members where you would normally use `T&`. It
enforces that the reference is never null.

## Key Differences from Native References

### Rebinding

Unlike `T&`, a mutable `raw_ref<T>` can be reassigned:

- `const raw_ref<T>`: Reference cannot be rebound.
- `const raw_ref<const T>`: Read-only reference; cannot be rebound.

### Use After Move

`raw_ref<T>` will safely abort if accessed after being moved.

## Traits

`raw_ref<T, Traits>` supports the same `RawPtrTraits` (e.g., `kMayDangle`) as
`raw_ptr`. See the [RawPtrTraits section in raw_ptr.md](./raw_ptr.md#RawPtrTraits).
