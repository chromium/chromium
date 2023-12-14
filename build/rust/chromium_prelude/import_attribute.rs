// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use proc_macro2::Span;
use quote::quote;
use syn::parse::{Parse, ParseStream};
use syn::{parse_macro_input, Error, Ident, Lit, Result, Token};

#[proc_macro]
pub fn import(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let imports = parse_macro_input!(input as ImportList).imports;

    let mut stream = proc_macro2::TokenStream::new();
    for i in imports {
        let public = &i.reexport;
        let mangled_crate_name = &i.target.mangled_crate_name;
        let name = i.alias.as_ref().unwrap_or(&i.target.gn_name);
        stream.extend(quote! {
          #public extern crate #mangled_crate_name as #name;
        });
    }
    stream.into()
}

struct ImportList {
    imports: Vec<Import>,
}

struct Import {
    target: GnTarget,
    alias: Option<Ident>,
    reexport: Option<Token![pub]>,
}

struct GnTarget {
    mangled_crate_name: Ident,
    gn_name: Ident,
}

impl GnTarget {
    fn parse(s: &str, span: Span) -> std::result::Result<GnTarget, String> {
        if !s.starts_with("//") {
            return Err(String::from("expected absolute GN path (should start with //)"));
        }

        let mut path: Vec<&str> = s[2..].split('/').collect();

        let (mangled_crate_name, gn_name) = {
            if path[0..2] == ["third_party", "rust"] {
                return Err(String::from(
                    "import! macro should not be used for third_party crates",
                ));
            }

            let last = path.pop().unwrap();
            let (split_last, gn_name) = match last.split_once(':') {
                Some((last, name)) => (last, name),
                None => (last, last),
            };
            path.push(split_last);

            for p in &path {
                if p.contains(':') {
                    return Err(String::from("unexpected ':' in GN path component"));
                }
                if p.is_empty() {
                    return Err(String::from("unexpected empty GN path component"));
                }
                // Valid GN indentifiers:
                // https://gn.googlesource.com/gn/+/main/docs/reference.md#identifiers
                //
                // We assume all path components are also valid GN identifiers, which
                // we believe they are in Chromium. This could be loosened if needed.
                if !p.chars().all(|c| c.is_ascii_alphanumeric() || c == '_') {
                    return Err(String::from(
                        "unexpected character in GN path component, \
                         expected characters in [A-Za-z0-9_]",
                    ));
                }
            }

            let mangled_crate_name: Vec<&str> =
                path.into_iter().chain(std::iter::once(gn_name)).collect();
            (mangled_crate_name.join("_"), gn_name)
        };

        Ok(GnTarget {
            mangled_crate_name: Ident::new(&mangled_crate_name, span),
            gn_name: Ident::new(gn_name, span),
        })
    }
}

impl Parse for ImportList {
    fn parse(input: ParseStream) -> Result<Self> {
        let mut imports: Vec<Import> = Vec::new();

        while !input.is_empty() {
            let reexport = <Token![pub]>::parse(input).ok();

            let str_span = input.span();

            let target = match Lit::parse(input) {
                Err(_) => {
                    return Err(Error::new(str_span, "expected a GN path as a string literal"));
                }
                Ok(Lit::Str(label)) => match GnTarget::parse(&label.value(), str_span) {
                    Ok(target) => target,
                    Err(e) => {
                        return Err(Error::new(
                            str_span,
                            format!("invalid GN path {}: {}", quote::quote! {#label}, e),
                        ));
                    }
                },
                Ok(lit) => {
                    return Err(Error::new(
                        str_span,
                        format!(
                            "expected a GN path as string literal, found '{}' literal",
                            quote::quote! {#lit}
                        ),
                    ));
                }
            };
            let alias = match <Token![as]>::parse(input) {
                Ok(_) => Some(Ident::parse(input)?),
                Err(_) => None,
            };
            <syn::Token![;]>::parse(input)?;

            imports.push(Import { target, alias, reexport });
        }

        Ok(Self { imports })
    }
}
