# Hashing

A hash function turns a variable-length input (called the "message", usually
`m`) into a fixed-length value (called the "hash", usually `h` or `H(m)`). Good
hash functions have the property that for two messages m0 and m1, if m0 differs
in any bit from m1, `H(m0)` and `H(m1)` are likely to differ in many bits.

This directory exports two recommended hash functions: a fast hash function and
a persistent hash function. The fast hash function is updated regularly as
faster hash functions become available, while the persistent hash function is
permanently frozen. That means that the value of the fast hash function for a
given message may change between Chromium revisions, but the value of the
persistent hash function for a given message will never change.

These are called `base::FastHash` and `base::PersistentHash` respectively and
are in [base/hash].

## Cryptographic Hashing

If you need cryptographic strength from your hash function, meaning that you
need it to be the case that either:

* Given `h`, nobody can find an `m` such that `H(m) = h`, or
* Given `m`, nobody can find an `m'` such that `H(m) = H(m')`

Then you need to use a cryptographic hash instead of one of the hashes here -
see [crypto/hash].

This directory contains implementations of two hash functions (MD5 and SHA-1)
which were previously considered cryptographically strong, but they **are no
longer considered secure** and you must not add new uses of them. See
[crypto/hash] for more details and suggested alternatives.

[base/hash]: hash.h
[crypto/hash]: ../../crypto/hash.h
