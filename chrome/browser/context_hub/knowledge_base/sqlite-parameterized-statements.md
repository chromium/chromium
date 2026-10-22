---
name: context-hub/sqlite-parameterized-statements
rubric: security
severity: blocking
---

**Rule.** In `MemoryBankTable` and other Context Hub SQLite table classes, bind
all dynamic SQL values using `sql::Statement` parameter binding methods
(`BindInt64`, `BindInt`, `BindTime`, `BindString`, `BindNull`) with `?`
placeholders or `sql::Cached*Builder` helpers (`sql::CachedInsertBuilder`,
`sql::CachedSelectBuilder`, `sql::CachedDeleteBuilder`) rather than string
concatenation.

**Why.** Constructing SQL queries by concatenating user- or page-supplied
values (such as URLs, tab titles, notes, collections, or tags) risks SQL
injection and causes query syntax failures when strings contain quotes or
special characters.

## Examples

Bad:

```cpp
std::optional<MemoryBankEntry> MemoryBankTable::GetEntry(int64_t id) {
  if (!db_) {
    return std::nullopt;
  }
  std::string query = base::StrCat(
      {"SELECT * FROM memory_bank_entries WHERE id = ",
       base::NumberToString(id)});
  sql::Statement statement(db_->GetUniqueStatement(query));
  // ...
}
```

Good:

```cpp
std::optional<MemoryBankEntry> MemoryBankTable::GetEntry(int64_t id) {
  if (!db_) {
    return std::nullopt;
  }
  sql::Statement statement;
  sql::CachedSelectBuilder(
      SQL_FROM_HERE, *db_, statement, kMemoryBankEntriesTable,
      /*columns=*/
      {kIdColumn, kTypeColumn, kTimestampColumn, kUrlColumn, kTabTitleColumn,
       kSelectedTextColumn, kTagsColumn, kNoteColumn, kCollectionColumn,
       kImageUrlColumn},
      /*modifiers=*/"WHERE id = ?");
  statement.BindInt64(0, id);
  if (!statement.Step()) {
    return std::nullopt;
  }
  return ToMemoryBankEntry(statement);
}
```
