---
name: general/test-empty-boundary-state-for-query-apis
rubric: testing
severity: should_fix
---

**Rule.** When writing unit tests for query, lookup, search, or membership
methods on containers, maps, or resource objects, explicitly include test cases
for empty objects (having zero elements or attributes). Do not rely solely on
negative queries against populated objects, as empty instances frequently follow
distinct initialization, allocation, or traversal code paths.

**Why.** Querying an empty container or zero-attribute resource often exercises
distinct boundary code paths—such as handling null buffers, empty spans, or
zero-length loops—compared to missing-key queries on populated instances.
Testing only negative lookups on non-empty objects can conceal null-pointer
dereferences, unhandled empty spans, or unexpected default return values.

## Examples

Bad:

```cpp
TEST(AccessTokenTest, HasSecurityAttribute) {
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  // Tests positive and negative lookups only on a token with existing attributes:
  EXPECT_TRUE(token->HasSecurityAttribute(L"TSA://ProcUnique").value_or(false));
  EXPECT_FALSE(token->HasSecurityAttribute(L"InvalidSA").value_or(true));
}
```

Good:

```cpp
TEST(AccessTokenTest, AnonymousTokenHasNoSecurityAttributes) {
  ATL::CAccessToken atl_anon_token;
  ASSERT_TRUE(::ImpersonateAnonymousToken(::GetCurrentThread()));
  bool result = atl_anon_token.GetThreadToken(TOKEN_QUERY);
  ASSERT_TRUE(result);
  ::RevertToSelf();
  std::optional<AccessToken> anon_token =
      AccessToken::FromToken(atl_anon_token.GetHandle());
  ASSERT_TRUE(anon_token);
  // Explicitly verifies querying an object with zero attributes:
  EXPECT_FALSE(
      anon_token->HasSecurityAttribute(L"TSA://ProcUnique").value_or(true));
}
```
