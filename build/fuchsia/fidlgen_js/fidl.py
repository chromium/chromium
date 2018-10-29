# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This was generated (and can be regenerated) by pasting
# zircon/system/host/fidl/schema.json from Fuchsia into
# https://app.quicktype.io and choosing Python 2.7 output. The only manual
# change is to modify the import path for Enum.

from third_party.enum34 import Enum


def from_str(x):
    assert isinstance(x, (str, unicode))
    return x


def from_int(x):
    assert isinstance(x, int) and not isinstance(x, bool)
    return x


def from_none(x):
    assert x is None
    return x


def from_union(fs, x):
    for f in fs:
        try:
            return f(x)
        except:
            pass
    assert False


def from_bool(x):
    assert isinstance(x, bool)
    return x


def to_class(c, x):
    assert isinstance(x, c)
    return x.to_dict()


def to_enum(c, x):
    assert isinstance(x, c)
    return x.value


def from_list(f, x):
    assert isinstance(x, list)
    return [f(y) for y in x]


def from_dict(f, x):
    assert isinstance(x, dict)
    return { k: f(v) for (k, v) in x.items() }


class Attribute:
    def __init__(self, name, value):
        self.name = name
        self.value = value

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        name = from_str(obj.get(u"name"))
        value = from_str(obj.get(u"value"))
        return Attribute(name, value)

    def to_dict(self):
        result = {}
        result[u"name"] = from_str(self.name)
        result[u"value"] = from_str(self.value)
        return result


class TypeKind(Enum):
    ARRAY = u"array"
    HANDLE = u"handle"
    IDENTIFIER = u"identifier"
    PRIMITIVE = u"primitive"
    REQUEST = u"request"
    STRING = u"string"
    VECTOR = u"vector"


class TypeClass:
    def __init__(self, element_count, element_type, kind, maybe_element_count, nullable, subtype, identifier):
        self.element_count = element_count
        self.element_type = element_type
        self.kind = kind
        self.maybe_element_count = maybe_element_count
        self.nullable = nullable
        self.subtype = subtype
        self.identifier = identifier

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        element_count = from_union([from_int, from_none], obj.get(u"element_count"))
        element_type = from_union([TypeClass.from_dict, from_none], obj.get(u"element_type"))
        kind = TypeKind(obj.get(u"kind"))
        maybe_element_count = from_union([from_int, from_none], obj.get(u"maybe_element_count"))
        nullable = from_union([from_bool, from_none], obj.get(u"nullable"))
        subtype = from_union([from_str, from_none], obj.get(u"subtype"))
        identifier = from_union([from_str, from_none], obj.get(u"identifier"))
        return TypeClass(element_count, element_type, kind, maybe_element_count, nullable, subtype, identifier)

    def to_dict(self):
        result = {}
        result[u"element_count"] = from_union([from_int, from_none], self.element_count)
        result[u"element_type"] = from_union([lambda x: to_class(TypeClass, x), from_none], self.element_type)
        result[u"kind"] = to_enum(TypeKind, self.kind)
        result[u"maybe_element_count"] = from_union([from_int, from_none], self.maybe_element_count)
        result[u"nullable"] = from_union([from_bool, from_none], self.nullable)
        result[u"subtype"] = from_union([from_str, from_none], self.subtype)
        result[u"identifier"] = from_union([from_str, from_none], self.identifier)
        return result


class ConstantKind(Enum):
    IDENTIFIER = u"identifier"
    LITERAL = u"literal"


class LiteralKind(Enum):
    DEFAULT = u"default"
    FALSE = u"false"
    NUMERIC = u"numeric"
    STRING = u"string"
    TRUE = u"true"


class Literal:
    def __init__(self, kind, value):
        self.kind = kind
        self.value = value

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        kind = LiteralKind(obj.get(u"kind"))
        value = from_union([from_str, from_none], obj.get(u"value"))
        return Literal(kind, value)

    def to_dict(self):
        result = {}
        result[u"kind"] = to_enum(LiteralKind, self.kind)
        result[u"value"] = from_union([from_str, from_none], self.value)
        return result


class Constant:
    def __init__(self, identifier, kind, literal):
        self.identifier = identifier
        self.kind = kind
        self.literal = literal

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        identifier = from_union([from_str, from_none], obj.get(u"identifier"))
        kind = ConstantKind(obj.get(u"kind"))
        literal = from_union([Literal.from_dict, from_none], obj.get(u"literal"))
        return Constant(identifier, kind, literal)

    def to_dict(self):
        result = {}
        result[u"identifier"] = from_union([from_str, from_none], self.identifier)
        result[u"kind"] = to_enum(ConstantKind, self.kind)
        result[u"literal"] = from_union([lambda x: to_class(Literal, x), from_none], self.literal)
        return result


class Const:
    def __init__(self, maybe_attributes, name, type, value):
        self.maybe_attributes = maybe_attributes
        self.name = name
        self.type = type
        self.value = value

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        maybe_attributes = from_union([lambda x: from_list(Attribute.from_dict, x), from_none], obj.get(u"maybe_attributes"))
        name = from_str(obj.get(u"name"))
        type = TypeClass.from_dict(obj.get(u"type"))
        value = Constant.from_dict(obj.get(u"value"))
        return Const(maybe_attributes, name, type, value)

    def to_dict(self):
        result = {}
        result[u"maybe_attributes"] = from_union([lambda x: from_list(lambda x: to_class(Attribute, x), x), from_none], self.maybe_attributes)
        result[u"name"] = from_str(self.name)
        result[u"type"] = to_class(TypeClass, self.type)
        result[u"value"] = to_class(Constant, self.value)
        return result


class DeclarationsMap(Enum):
    CONST = u"const"
    ENUM = u"enum"
    INTERFACE = u"interface"
    STRUCT = u"struct"
    UNION = u"union"


class EnumMember:
    def __init__(self, name, value):
        self.name = name
        self.value = value

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        name = from_str(obj.get(u"name"))
        value = Constant.from_dict(obj.get(u"value"))
        return EnumMember(name, value)

    def to_dict(self):
        result = {}
        result[u"name"] = from_str(self.name)
        result[u"value"] = to_class(Constant, self.value)
        return result


class IntegerType(Enum):
    INT16 = u"int16"
    INT32 = u"int32"
    INT64 = u"int64"
    INT8 = u"int8"
    UINT16 = u"uint16"
    UINT32 = u"uint32"
    UINT64 = u"uint64"
    UINT8 = u"uint8"


class EnumDeclarationElement:
    def __init__(self, maybe_attributes, members, name, type):
        self.maybe_attributes = maybe_attributes
        self.members = members
        self.name = name
        self.type = type

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        maybe_attributes = from_union([lambda x: from_list(Attribute.from_dict, x), from_none], obj.get(u"maybe_attributes"))
        members = from_list(EnumMember.from_dict, obj.get(u"members"))
        name = from_str(obj.get(u"name"))
        type = IntegerType(obj.get(u"type"))
        return EnumDeclarationElement(maybe_attributes, members, name, type)

    def to_dict(self):
        result = {}
        result[u"maybe_attributes"] = from_union([lambda x: from_list(lambda x: to_class(Attribute, x), x), from_none], self.maybe_attributes)
        result[u"members"] = from_list(lambda x: to_class(EnumMember, x), self.members)
        result[u"name"] = from_str(self.name)
        result[u"type"] = to_enum(IntegerType, self.type)
        return result


class InterfaceMethodParameter:
    def __init__(self, alignment, name, offset, size, type):
        self.alignment = alignment
        self.name = name
        self.offset = offset
        self.size = size
        self.type = type

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        alignment = from_int(obj.get(u"alignment"))
        name = from_str(obj.get(u"name"))
        offset = from_int(obj.get(u"offset"))
        size = from_int(obj.get(u"size"))
        type = TypeClass.from_dict(obj.get(u"type"))
        return InterfaceMethodParameter(alignment, name, offset, size, type)

    def to_dict(self):
        result = {}
        result[u"alignment"] = from_int(self.alignment)
        result[u"name"] = from_str(self.name)
        result[u"offset"] = from_int(self.offset)
        result[u"size"] = from_int(self.size)
        result[u"type"] = to_class(TypeClass, self.type)
        return result


class InterfaceMethod:
    def __init__(self, has_request, has_response, maybe_attributes, maybe_request, maybe_request_alignment, maybe_request_size, maybe_response, maybe_response_alignment, maybe_response_size, name, ordinal):
        self.has_request = has_request
        self.has_response = has_response
        self.maybe_attributes = maybe_attributes
        self.maybe_request = maybe_request
        self.maybe_request_alignment = maybe_request_alignment
        self.maybe_request_size = maybe_request_size
        self.maybe_response = maybe_response
        self.maybe_response_alignment = maybe_response_alignment
        self.maybe_response_size = maybe_response_size
        self.name = name
        self.ordinal = ordinal

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        has_request = from_bool(obj.get(u"has_request"))
        has_response = from_bool(obj.get(u"has_response"))
        maybe_attributes = from_union([lambda x: from_list(Attribute.from_dict, x), from_none], obj.get(u"maybe_attributes"))
        maybe_request = from_union([lambda x: from_list(InterfaceMethodParameter.from_dict, x), from_none], obj.get(u"maybe_request"))
        maybe_request_alignment = from_union([from_int, from_none], obj.get(u"maybe_request_alignment"))
        maybe_request_size = from_union([from_int, from_none], obj.get(u"maybe_request_size"))
        maybe_response = from_union([lambda x: from_list(InterfaceMethodParameter.from_dict, x), from_none], obj.get(u"maybe_response"))
        maybe_response_alignment = from_union([from_int, from_none], obj.get(u"maybe_response_alignment"))
        maybe_response_size = from_union([from_int, from_none], obj.get(u"maybe_response_size"))
        name = from_str(obj.get(u"name"))
        ordinal = from_int(obj.get(u"ordinal"))
        return InterfaceMethod(has_request, has_response, maybe_attributes, maybe_request, maybe_request_alignment, maybe_request_size, maybe_response, maybe_response_alignment, maybe_response_size, name, ordinal)

    def to_dict(self):
        result = {}
        result[u"has_request"] = from_bool(self.has_request)
        result[u"has_response"] = from_bool(self.has_response)
        result[u"maybe_attributes"] = from_union([lambda x: from_list(lambda x: to_class(Attribute, x), x), from_none], self.maybe_attributes)
        result[u"maybe_request"] = from_union([lambda x: from_list(lambda x: to_class(InterfaceMethodParameter, x), x), from_none], self.maybe_request)
        result[u"maybe_request_alignment"] = from_union([from_int, from_none], self.maybe_request_alignment)
        result[u"maybe_request_size"] = from_union([from_int, from_none], self.maybe_request_size)
        result[u"maybe_response"] = from_union([lambda x: from_list(lambda x: to_class(InterfaceMethodParameter, x), x), from_none], self.maybe_response)
        result[u"maybe_response_alignment"] = from_union([from_int, from_none], self.maybe_response_alignment)
        result[u"maybe_response_size"] = from_union([from_int, from_none], self.maybe_response_size)
        result[u"name"] = from_str(self.name)
        result[u"ordinal"] = from_int(self.ordinal)
        return result


class Interface:
    def __init__(self, maybe_attributes, methods, name):
        self.maybe_attributes = maybe_attributes
        self.methods = methods
        self.name = name

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        maybe_attributes = from_union([lambda x: from_list(Attribute.from_dict, x), from_none], obj.get(u"maybe_attributes"))
        methods = from_list(InterfaceMethod.from_dict, obj.get(u"methods"))
        name = from_str(obj.get(u"name"))
        return Interface(maybe_attributes, methods, name)

    def to_dict(self):
        result = {}
        result[u"maybe_attributes"] = from_union([lambda x: from_list(lambda x: to_class(Attribute, x), x), from_none], self.maybe_attributes)
        result[u"methods"] = from_list(lambda x: to_class(InterfaceMethod, x), self.methods)
        result[u"name"] = from_str(self.name)
        return result


class Library:
    def __init__(self, declarations, name):
        self.declarations = declarations
        self.name = name

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        declarations = from_dict(DeclarationsMap, obj.get(u"declarations"))
        name = from_str(obj.get(u"name"))
        return Library(declarations, name)

    def to_dict(self):
        result = {}
        result[u"declarations"] = from_dict(lambda x: to_enum(DeclarationsMap, x), self.declarations)
        result[u"name"] = from_str(self.name)
        return result


class StructMember:
    def __init__(self, alignment, maybe_default_value, name, offset, size, type):
        self.alignment = alignment
        self.maybe_default_value = maybe_default_value
        self.name = name
        self.offset = offset
        self.size = size
        self.type = type

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        alignment = from_int(obj.get(u"alignment"))
        maybe_default_value = from_union([Constant.from_dict, from_none], obj.get(u"maybe_default_value"))
        name = from_str(obj.get(u"name"))
        offset = from_int(obj.get(u"offset"))
        size = from_int(obj.get(u"size"))
        type = TypeClass.from_dict(obj.get(u"type"))
        return StructMember(alignment, maybe_default_value, name, offset, size, type)

    def to_dict(self):
        result = {}
        result[u"alignment"] = from_int(self.alignment)
        result[u"maybe_default_value"] = from_union([lambda x: to_class(Constant, x), from_none], self.maybe_default_value)
        result[u"name"] = from_str(self.name)
        result[u"offset"] = from_int(self.offset)
        result[u"size"] = from_int(self.size)
        result[u"type"] = to_class(TypeClass, self.type)
        return result


class Struct:
    def __init__(self, max_handles, maybe_attributes, members, name, size):
        self.max_handles = max_handles
        self.maybe_attributes = maybe_attributes
        self.members = members
        self.name = name
        self.size = size

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        max_handles = from_union([from_int, from_none], obj.get(u"max_handles"))
        maybe_attributes = from_union([lambda x: from_list(Attribute.from_dict, x), from_none], obj.get(u"maybe_attributes"))
        members = from_list(StructMember.from_dict, obj.get(u"members"))
        name = from_str(obj.get(u"name"))
        size = from_int(obj.get(u"size"))
        return Struct(max_handles, maybe_attributes, members, name, size)

    def to_dict(self):
        result = {}
        result[u"max_handles"] = from_union([from_int, from_none], self.max_handles)
        result[u"maybe_attributes"] = from_union([lambda x: from_list(lambda x: to_class(Attribute, x), x), from_none], self.maybe_attributes)
        result[u"members"] = from_list(lambda x: to_class(StructMember, x), self.members)
        result[u"name"] = from_str(self.name)
        result[u"size"] = from_int(self.size)
        return result


class UnionMember:
    def __init__(self, alignment, name, offset, size, type):
        self.alignment = alignment
        self.name = name
        self.offset = offset
        self.size = size
        self.type = type

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        alignment = from_int(obj.get(u"alignment"))
        name = from_str(obj.get(u"name"))
        offset = from_int(obj.get(u"offset"))
        size = from_int(obj.get(u"size"))
        type = TypeClass.from_dict(obj.get(u"type"))
        return UnionMember(alignment, name, offset, size, type)

    def to_dict(self):
        result = {}
        result[u"alignment"] = from_int(self.alignment)
        result[u"name"] = from_str(self.name)
        result[u"offset"] = from_int(self.offset)
        result[u"size"] = from_int(self.size)
        result[u"type"] = to_class(TypeClass, self.type)
        return result


class UnionDeclarationElement:
    def __init__(self, alignment, max_handles, maybe_attributes, members, name, size):
        self.alignment = alignment
        self.max_handles = max_handles
        self.maybe_attributes = maybe_attributes
        self.members = members
        self.name = name
        self.size = size

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        alignment = from_int(obj.get(u"alignment"))
        max_handles = from_union([from_int, from_none], obj.get(u"max_handles"))
        maybe_attributes = from_union([lambda x: from_list(Attribute.from_dict, x), from_none], obj.get(u"maybe_attributes"))
        members = from_list(UnionMember.from_dict, obj.get(u"members"))
        name = from_str(obj.get(u"name"))
        size = from_int(obj.get(u"size"))
        return UnionDeclarationElement(alignment, max_handles, maybe_attributes, members, name, size)

    def to_dict(self):
        result = {}
        result[u"alignment"] = from_int(self.alignment)
        result[u"max_handles"] = from_union([from_int, from_none], self.max_handles)
        result[u"maybe_attributes"] = from_union([lambda x: from_list(lambda x: to_class(Attribute, x), x), from_none], self.maybe_attributes)
        result[u"members"] = from_list(lambda x: to_class(UnionMember, x), self.members)
        result[u"name"] = from_str(self.name)
        result[u"size"] = from_int(self.size)
        return result


class Fidl:
    def __init__(self, const_declarations, declaration_order, declarations, enum_declarations, interface_declarations, library_dependencies, name, struct_declarations, union_declarations, version):
        self.const_declarations = const_declarations
        self.declaration_order = declaration_order
        self.declarations = declarations
        self.enum_declarations = enum_declarations
        self.interface_declarations = interface_declarations
        self.library_dependencies = library_dependencies
        self.name = name
        self.struct_declarations = struct_declarations
        self.union_declarations = union_declarations
        self.version = version

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        const_declarations = from_list(Const.from_dict, obj.get(u"const_declarations"))
        declaration_order = from_list(from_str, obj.get(u"declaration_order"))
        declarations = from_dict(DeclarationsMap, obj.get(u"declarations"))
        enum_declarations = from_list(EnumDeclarationElement.from_dict, obj.get(u"enum_declarations"))
        interface_declarations = from_list(Interface.from_dict, obj.get(u"interface_declarations"))
        library_dependencies = from_list(Library.from_dict, obj.get(u"library_dependencies"))
        name = from_str(obj.get(u"name"))
        struct_declarations = from_list(Struct.from_dict, obj.get(u"struct_declarations"))
        union_declarations = from_list(UnionDeclarationElement.from_dict, obj.get(u"union_declarations"))
        version = from_str(obj.get(u"version"))
        return Fidl(const_declarations, declaration_order, declarations, enum_declarations, interface_declarations, library_dependencies, name, struct_declarations, union_declarations, version)

    def to_dict(self):
        result = {}
        result[u"const_declarations"] = from_list(lambda x: to_class(Const, x), self.const_declarations)
        result[u"declaration_order"] = from_list(from_str, self.declaration_order)
        result[u"declarations"] = from_dict(lambda x: to_enum(DeclarationsMap, x), self.declarations)
        result[u"enum_declarations"] = from_list(lambda x: to_class(EnumDeclarationElement, x), self.enum_declarations)
        result[u"interface_declarations"] = from_list(lambda x: to_class(Interface, x), self.interface_declarations)
        result[u"library_dependencies"] = from_list(lambda x: to_class(Library, x), self.library_dependencies)
        result[u"name"] = from_str(self.name)
        result[u"struct_declarations"] = from_list(lambda x: to_class(Struct, x), self.struct_declarations)
        result[u"union_declarations"] = from_list(lambda x: to_class(UnionDeclarationElement, x), self.union_declarations)
        result[u"version"] = from_str(self.version)
        return result


def fidl_from_dict(s):
    return Fidl.from_dict(s)


def fidl_to_dict(x):
    return to_class(Fidl, x)

